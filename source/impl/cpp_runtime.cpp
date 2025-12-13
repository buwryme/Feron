// source/impl/cpp_runtime.cpp
// Minimal freestanding C++ runtime: heap allocator, new/delete, and small libc helpers.

#include <cstddef>
#include <cstdint>

// stubbed std helpers used by new/delete signatures
namespace std {
    struct nothrow_t {};
    inline constexpr nothrow_t nothrow{};

    struct align_val_t {
        explicit align_val_t(std::size_t a) : value(a) {}
        std::size_t value;
    };
}

extern "C" {

// -----------------------------
// Kernel heap management state
// -----------------------------
static unsigned char* heap_start = nullptr;
static unsigned char* heap_end   = nullptr;

static struct HeapConfig {
    bool initialized = false;
    std::size_t total_size = 0;
} heap_config;

// call this once with your memory region
void kernel_heap_init(void* addr, std::size_t size) {
    if (!addr || size < 64) return; // sanity
    heap_start = reinterpret_cast<unsigned char*>(addr);
    heap_end   = heap_start + size;
    heap_config.initialized = true;
    heap_config.total_size = size;
}

// -----------------------------
// Low-level helpers
// -----------------------------
static inline std::size_t align_up(std::size_t n, std::size_t a) {
    return (n + (a - 1)) & ~(a - 1);
}

// spinlock for simple thread-safety (single-core safe for now)
static volatile uint8_t allocator_lock_flag = 0;
static inline void spin_lock() {
    while (__atomic_test_and_set(&allocator_lock_flag, __ATOMIC_ACQUIRE)) {}
}
static inline void spin_unlock() {
    __atomic_clear(&allocator_lock_flag, __ATOMIC_RELEASE);
}

// -----------------------------
// Block layout and freelist
// -----------------------------
struct BlockHeader {
    std::size_t size_and_flag; // total block size (including header+payload+footer) | low bit = allocated
    BlockHeader* next_free;
    BlockHeader* prev_free;
};

// We'll compute an aligned header size at allocator init and use it consistently.
static std::size_t header_size_aligned = 0;

static inline std::size_t footer_size() { return sizeof(std::size_t); }
static inline std::size_t min_payload() { return 16; }
static inline std::size_t min_block_size() {
    // header + footer + min payload
    return header_size_aligned + footer_size() + min_payload();
}

static BlockHeader* free_list_head = nullptr;

static inline bool is_allocated(const BlockHeader* h) {
    return (h->size_and_flag & 1u) != 0;
}
static inline void set_allocated(BlockHeader* h, bool a) {
    if (a) h->size_and_flag |= 1u;
    else    h->size_and_flag &= ~static_cast<std::size_t>(1u);
}
static inline std::size_t block_size(const BlockHeader* h) {
    return h->size_and_flag & ~static_cast<std::size_t>(1u);
}
static inline void write_footer(BlockHeader* h) {
    std::size_t sz = block_size(h);
    unsigned char* footer_pos = reinterpret_cast<unsigned char*>(h) + sz - sizeof(std::size_t);
    *reinterpret_cast<std::size_t*>(footer_pos) = sz;
}
static inline BlockHeader* next_phys(BlockHeader* h) {
    unsigned char* next = reinterpret_cast<unsigned char*>(h) + block_size(h);
    if (next >= heap_end) return nullptr;
    return reinterpret_cast<BlockHeader*>(next);
}
static inline BlockHeader* prev_phys(BlockHeader* h) {
    unsigned char* hdr_ptr = reinterpret_cast<unsigned char*>(h);
    if (hdr_ptr == heap_start) return nullptr;
    unsigned char* prev_footer_pos = hdr_ptr - sizeof(std::size_t);
    std::size_t prev_size = *reinterpret_cast<std::size_t*>(prev_footer_pos);
    if (prev_size == 0) return nullptr;
    unsigned char* prev_hdr_pos = hdr_ptr - prev_size;
    if (prev_hdr_pos < heap_start) return nullptr;
    return reinterpret_cast<BlockHeader*>(prev_hdr_pos);
}

// freelist helpers
static inline void remove_from_freelist(BlockHeader* b) {
    if (!b) return;
    if (b->prev_free) b->prev_free->next_free = b->next_free;
    else free_list_head = b->next_free;
    if (b->next_free) b->next_free->prev_free = b->prev_free;
    b->next_free = b->prev_free = nullptr;
}
static inline void insert_into_freelist(BlockHeader* b) {
    b->next_free = free_list_head;
    if (free_list_head) free_list_head->prev_free = b;
    b->prev_free = nullptr;
    free_list_head = b;
}

// allocator initialization
static inline void allocator_init() {
    if (!heap_config.initialized) return;
    if (header_size_aligned == 0) {
        header_size_aligned = align_up(sizeof(BlockHeader), alignof(std::max_align_t));
    }
    if (free_list_head != nullptr) return;
    BlockHeader* initial = reinterpret_cast<BlockHeader*>(heap_start);
    initial->size_and_flag = (heap_end - heap_start) & ~static_cast<std::size_t>(1u); // free
    initial->next_free = nullptr;
    initial->prev_free = nullptr;
    write_footer(initial);
    free_list_head = initial;
}

// -----------------------------
// Allocation / Free implementation
// -----------------------------
static void* allocator_alloc(std::size_t payload_size, std::size_t /*alignment*/) {
    if (!heap_config.initialized) return nullptr;
    if (payload_size == 0) payload_size = 1;

    // Use a fixed payload offset equal to the aligned header size.
    // This keeps header <-> payload arithmetic simple and consistent for free().
    std::size_t payload_offset = header_size_aligned;
    std::size_t total_needed = payload_offset + payload_size + footer_size();
    total_needed = align_up(total_needed, alignof(std::max_align_t));
    if (total_needed < min_block_size()) total_needed = min_block_size();

    spin_lock();
    allocator_init();
    BlockHeader* cur = free_list_head;
    while (cur) {
        std::size_t cur_sz = block_size(cur);
        if (cur_sz >= total_needed) {
            std::size_t remaining = cur_sz - total_needed;
            if (remaining >= min_block_size()) {
                // split block: cur becomes allocated block of size total_needed
                unsigned char* next_pos = reinterpret_cast<unsigned char*>(cur) + total_needed;
                BlockHeader* new_free = reinterpret_cast<BlockHeader*>(next_pos);
                new_free->size_and_flag = remaining & ~static_cast<std::size_t>(1u);
                write_footer(new_free);

                // replace cur in free list with new_free
                new_free->next_free = cur->next_free;
                if (new_free->next_free) new_free->next_free->prev_free = new_free;
                new_free->prev_free = cur->prev_free;
                if (new_free->prev_free) new_free->prev_free->next_free = new_free;
                else free_list_head = new_free;

                // shrink cur
                cur->size_and_flag = total_needed & ~static_cast<std::size_t>(1u);
                write_footer(cur);
            } else {
                // use entire block
                remove_from_freelist(cur);
            }
            set_allocated(cur, true);
            write_footer(cur);
            spin_unlock();
            unsigned char* user_ptr = reinterpret_cast<unsigned char*>(cur) + payload_offset;
            return static_cast<void*>(user_ptr);
        }
        cur = cur->next_free;
    }
    spin_unlock();
    return nullptr; // out of memory
}

static void allocator_coalesce_and_free(BlockHeader* h) {
    set_allocated(h, false);
    write_footer(h);

    BlockHeader* nx = next_phys(h);
    if (nx && !is_allocated(nx)) {
        remove_from_freelist(nx);
        std::size_t newsize = block_size(h) + block_size(nx);
        h->size_and_flag = newsize & ~static_cast<std::size_t>(1u);
        write_footer(h);
    }

    BlockHeader* pv = prev_phys(h);
    if (pv && !is_allocated(pv)) {
        remove_from_freelist(pv);
        std::size_t newsize = block_size(pv) + block_size(h);
        pv->size_and_flag = newsize & ~static_cast<std::size_t>(1u);
        write_footer(pv);
        insert_into_freelist(pv);
    } else {
        insert_into_freelist(h);
    }
}

static void allocator_free(void* ptr) {
    if (!ptr) return;
    if (!heap_config.initialized) return;

    unsigned char* u = reinterpret_cast<unsigned char*>(ptr);
    if (u < heap_start + header_size_aligned) return; // too small to be valid
    unsigned char* hdr_candidate = u - header_size_aligned;
    if (hdr_candidate < heap_start) return;

    BlockHeader* h = reinterpret_cast<BlockHeader*>(hdr_candidate);
    // basic sanity: block size must be reasonable
    std::size_t sz = block_size(h);
    if (sz == 0 || hdr_candidate + sz > heap_end) return;

    spin_lock();
    allocator_coalesce_and_free(h);
    spin_unlock();
}

// -----------------------------
// Public C API (malloc/free/calloc/realloc)
// -----------------------------
void* malloc(std::size_t size) { return allocator_alloc(size, alignof(std::max_align_t)); }
void free(void* ptr) { allocator_free(ptr); }
void* calloc(std::size_t nmemb, std::size_t size) {
    std::size_t total = nmemb * size;
    void* p = malloc(total);
    if (p) {
        unsigned char* d = reinterpret_cast<unsigned char*>(p);
        for (std::size_t i = 0; i < total; ++i) d[i] = 0;
    }
    return p;
}
void* realloc(void* ptr, std::size_t newsize) {
    if (!ptr) return malloc(newsize);
    if (newsize == 0) { free(ptr); return nullptr; }
    void* newptr = malloc(newsize);
    if (!newptr) return nullptr;
    // simple copy; find old size
    unsigned char* hdr_candidate = reinterpret_cast<unsigned char*>(ptr) - header_size_aligned;
    BlockHeader* h = reinterpret_cast<BlockHeader*>(hdr_candidate);
    std::size_t oldsize = block_size(h) - header_size_aligned - footer_size();
    std::size_t tocopy = (oldsize < newsize) ? oldsize : newsize;
    unsigned char* s = reinterpret_cast<unsigned char*>(ptr);
    unsigned char* d = reinterpret_cast<unsigned char*>(newptr);
    for (std::size_t i = 0; i < tocopy; ++i) d[i] = s[i];
    free(ptr);
    return newptr;
}

} // extern "C"

// -----------------------------
// C++ new/delete overloads
// -----------------------------
extern "C++" {

void* operator new(std::size_t size) { return malloc(size); }
void* operator new[](std::size_t size) { return operator new(size); }

void* operator new(std::size_t size, const std::nothrow_t&) noexcept { return malloc(size); }
void* operator new[](std::size_t size, const std::nothrow_t&) noexcept { return malloc(size); }

void* operator new(std::size_t size, const std::align_val_t /*al*/) { return malloc(size); }
void* operator new[](std::size_t size, const std::align_val_t /*al*/) { return operator new(size); }

void* operator new(std::size_t size, const std::align_val_t /*al*/, const std::nothrow_t&) noexcept { return malloc(size); }
void* operator new[](std::size_t size, const std::align_val_t /*al*/, const std::nothrow_t&) noexcept { return operator new(size); }

void operator delete(void* ptr) noexcept { free(ptr); }
void operator delete[](void* ptr) noexcept { free(ptr); }

void operator delete(void* ptr, std::size_t) noexcept { free(ptr); }
void operator delete[](void* ptr, std::size_t) noexcept { free(ptr); }

void operator delete(void* ptr, const std::align_val_t) noexcept { free(ptr); }
void operator delete[](void* ptr, const std::align_val_t) noexcept { free(ptr); }

void operator delete(void* ptr, std::size_t, const std::align_val_t) noexcept { free(ptr); }
void operator delete[](void* ptr, std::size_t, const std::align_val_t) noexcept { free(ptr); }

} // extern "C++"

// -----------------------------
// C++ runtime support
// -----------------------------
void __cxa_pure_virtual() { for (;;) {} }

// guard helpers for static local initialization
int __cxa_guard_acquire(uint64_t* g) {
    if ((*g & 1ull) != 0) return 0;
    uint64_t expected = 0;
    uint64_t desired = 2ull;
    if (__atomic_compare_exchange_n(g, &expected, desired, false, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE))
        return 1;
    while ((*g & 1ull) == 0) {}
    return 0;
}
void __cxa_guard_release(uint64_t* g) { __atomic_fetch_or(g, 1ull, __ATOMIC_RELEASE); }
void __cxa_guard_abort(uint64_t* g) { __atomic_and_fetch(g, ~2ull, __ATOMIC_RELEASE); }

// atexit/finalize (simple table)
using at_exit_fn = void(*)(void*);
struct AtexitEntry { at_exit_fn fn; void* obj; void* dso; };
constexpr std::size_t ATEXIT_CAP = 256;
static AtexitEntry atexit_table[ATEXIT_CAP];
static std::size_t atexit_count = 0;
int __cxa_atexit(void (*f)(void*), void* p, void* d) {
    if (!f) return 1;
    spin_lock();
    if (atexit_count >= ATEXIT_CAP) { spin_unlock(); return 1; }
    atexit_table[atexit_count++] = { f,p,d };
    spin_unlock();
    return 0;
}
void __cxa_finalize(void* d) {
    spin_lock();
    for (std::ptrdiff_t i = static_cast<std::ptrdiff_t>(atexit_count) - 1; i >= 0; --i) {
        if (d == nullptr || atexit_table[i].dso == d) {
            if (atexit_table[i].fn) atexit_table[i].fn(atexit_table[i].obj);
            atexit_table[i].fn = nullptr;
        }
    }
    spin_unlock();
}

// -----------------------------
// Minimal libc helpers (memcpy/memset/memmove)
// -----------------------------
extern "C" {

void* memcpy(void* dest, const void* src, std::size_t n) {
    unsigned char* d = reinterpret_cast<unsigned char*>(dest);
    const unsigned char* s = reinterpret_cast<const unsigned char*>(src);
    while (n--) *d++ = *s++;
    return dest;
}
void* memset(void* s, int c, std::size_t n) {
    unsigned char* d = reinterpret_cast<unsigned char*>(s);
    while (n--) *d++ = static_cast<unsigned char>(c);
    return s;
}
void* memmove(void* dest, const void* src, std::size_t n) {
    unsigned char* d = reinterpret_cast<unsigned char*>(dest);
    const unsigned char* s = reinterpret_cast<const unsigned char*>(src);
    if (d < s) return memcpy(dest, src, n);
    d += n; s += n;
    while (n--) *--d = *--s;
    return dest;
}

std::size_t strlen(const char* s) {
    if (!s) return 0;
    const char* p = s;
    while (*p) ++p;
    return static_cast<std::size_t>(p - s);
}

int memcmp(const void* a, const void* b, std::size_t n) {
    const unsigned char* x = reinterpret_cast<const unsigned char*>(a);
    const unsigned char* y = reinterpret_cast<const unsigned char*>(b);
    for (std::size_t i = 0; i < n; ++i) {
        if (x[i] < y[i]) return -1;
        if (x[i] > y[i]) return 1;
    }
    return 0;
}

// naive strstr (returns pointer to first occurrence or nullptr)
char* strstr(const char* haystack, const char* needle) {
    if (!haystack || !needle) return nullptr;
    if (*needle == '\0') return const_cast<char*>(haystack);
    std::size_t nlen = strlen(needle);
    for (const char* p = haystack; *p; ++p) {
        std::size_t i = 0;
        for (; i < nlen; ++i) {
            if (p[i] == '\0') return nullptr;
            if (p[i] != needle[i]) break;
        }
        if (i == nlen) return const_cast<char*>(p);
    }
    return nullptr;
}

// abort
void abort() { for (;;) {} }

} // extern "C"

// stack protector hook
extern "C" void __stack_chk_fail() {
    for (;;) {} // halt on stack smashing
}
