#pragma once

#include <cstdint>

namespace feron::mm::config {
    inline uint64_t va_pool_base = 0xFFFF800000000000ull;
    inline uint64_t va_pool_size = 1ull * 1024 * 1024; // 1 MiB
}
