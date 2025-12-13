#pragma once

#include <cstddef>

extern "C" {
    void* memmove (void* dest, const void *src, std::size_t n);
}