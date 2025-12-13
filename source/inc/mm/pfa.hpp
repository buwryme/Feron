#pragma once

#include <cstdint>
#include "../runtime/impl/mm/malloc.hpp"
#include "../runtime/impl/mem/set.hpp"
#include "../boot/mb2.hpp"

namespace feron::mm::pfa {
    // 4 KiB pages
    constexpr std::size_t PAGE_SIZE = 4096;

    // Simple global bitmap over all usable memory
    inline uint64_t phys_base = 0;       // first usable physical address
    inline uint64_t phys_limit = 0;      // end of usable range (exclusive)
    inline uint64_t total_pages = 0;

    inline uint8_t* bitmap = nullptr;    // points inside kernel heap
    inline uint64_t bitmap_bytes = 0;

    // Mark bit helpers
    inline bool bit_get(uint64_t i) { return (bitmap[i >> 3] >> (i & 7)) & 1; }
    inline void bit_set(uint64_t i) { bitmap[i >> 3] |=  (1u << (i & 7)); }
    inline void bit_clear(uint64_t i){ bitmap[i >> 3] &= ~(1u << (i & 7)); }

    // Returns page index for a physical address
    inline uint64_t pa_to_index(uint64_t pa) { return (pa - phys_base) / PAGE_SIZE; }
    inline uint64_t index_to_pa(uint64_t idx) { return phys_base + idx * PAGE_SIZE; }

    // Initialize from Multiboot2 mmap: choose the lowest usable region as base and cover all usable as range.
    // Allocate the bitmap from kernel heap.
    inline void init(const feron::boot::mb2::info_t& info) {
        uint64_t min_addr = UINT64_MAX, max_addr = 0;

        // Collect global usable range
        if (info.mmap && info.mmap_count > 0) {
            for (uint32_t i = 0; i < info.mmap_count; ++i) {
                auto e = info.mmap[i];
                if (e.type == 1 && e.len) {
                    if (e.addr < min_addr) min_addr = e.addr;
                    if (e.addr + e.len > max_addr) max_addr = e.addr + e.len;
                }
            }
        }

        if (min_addr == UINT64_MAX || max_addr <= min_addr) {
            // No usable memory reported: keep allocator disabled
            phys_base = phys_limit = total_pages = 0;
            bitmap = nullptr; bitmap_bytes = 0;
            return;
        }

        // Skip the first page for safety
        phys_base = (min_addr + PAGE_SIZE) & ~(uint64_t)(PAGE_SIZE - 1);
        phys_limit = max_addr & ~(uint64_t)(PAGE_SIZE - 1);
        if (phys_limit <= phys_base) {
            phys_base = phys_limit = total_pages = 0; bitmap = nullptr; bitmap_bytes = 0; return;
        }

        total_pages = (phys_limit - phys_base) / PAGE_SIZE;
        bitmap_bytes = (total_pages + 7) / 8;

        bitmap = static_cast<uint8_t*>(malloc(bitmap_bytes));
        if (!bitmap) { total_pages = 0; bitmap_bytes = 0; return; }
        memset(bitmap, 0, bitmap_bytes);

        // Reserve non-usable regions within range by marking their bits (type != 1)
        if (info.mmap && info.mmap_count > 0) {
            for (uint32_t i = 0; i < info.mmap_count; ++i) {
                auto e = info.mmap[i];
                if (e.type != 1 && e.len) {
                    uint64_t start = (e.addr < phys_base) ? phys_base : e.addr;
                    uint64_t end   = (e.addr + e.len > phys_limit) ? phys_limit : (e.addr + e.len);
                    if (end > start) {
                        uint64_t sidx = (start - phys_base) / PAGE_SIZE;
                        uint64_t eidx = (end   - phys_base) / PAGE_SIZE;
                        for (uint64_t idx = sidx; idx < eidx; ++idx) bit_set(idx);
                    }
                }
            }
        }

        // Reserve low critical pages: first few MB for identity, VGA (0xB8000), kernel image, etc.
        auto reserve_pa_range = [&](uint64_t start, uint64_t end){
            if (start < phys_base) start = phys_base;
            if (end   > phys_limit) end = phys_limit;
            if (end <= start) return;
            uint64_t sidx = (start - phys_base) / PAGE_SIZE;
            uint64_t eidx = (end   - phys_base) / PAGE_SIZE;
            for (uint64_t idx = sidx; idx < eidx; ++idx) bit_set(idx);
        };

        // Reserve first 16 MiB for boot identity and devices
        reserve_pa_range(phys_base, phys_base + 16ull * 1024 * 1024);

        // Reserve VGA text page (round to page)
        reserve_pa_range(0x00000000000B8000ull & ~(PAGE_SIZE - 1), (0xB8000ull & ~(PAGE_SIZE - 1)) + PAGE_SIZE);
    }

    // Allocate one free page (returns physical address or 0)
    inline uint64_t alloc_page() {
        if (!bitmap || total_pages == 0) return 0;
        for (uint64_t idx = 0; idx < total_pages; ++idx) {
            if (!bit_get(idx)) {
                bit_set(idx);
                return index_to_pa(idx);
            }
        }
        return 0; // out of pages
    }

    inline void free_page(uint64_t pa) {
        if (!bitmap || pa < phys_base || pa >= phys_limit) return;
        uint64_t idx = pa_to_index(pa);
        bit_clear(idx);
    }
}
