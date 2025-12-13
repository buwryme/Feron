#pragma once

#include <cstdint>

namespace feron::mm::valloc {
    inline uint64_t va_base = 0;
    inline uint64_t va_end  = 0;
    inline uint64_t cursor  = 0;

    // Initialize allocator over [base, base+size)
    inline void init(uint64_t base, uint64_t size) {
        va_base = base;
        va_end  = base + size;
        cursor  = base;
    }

    // Allocate a contiguous VA range (size must be multiple of page size)
    inline uint64_t alloc_range(uint64_t size, uint64_t align = 4096) {
        uint64_t aligned = (cursor + (align - 1)) & ~(align - 1);
        if (aligned + size > va_end) return 0;
        cursor = aligned + size;
        return aligned;
    }
}
