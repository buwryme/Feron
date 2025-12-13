#pragma once

#include <cstddef>

extern "C" {
    // heap API used by kernel code
    void  kernel_heap_init(void* addr, std::size_t size);

    // allocator API
    void* malloc(std::size_t size);
    void  free(void* ptr);
    void* calloc(std::size_t nmemb, std::size_t size);
    void* realloc(void* ptr, std::size_t newsize);
}
