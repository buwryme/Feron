#pragma once

#include <cstddef>

extern "C" {
    void kernel_heap_init(void *addr, std::size_t size);
}