#pragma once

#include <cstddef>

extern "C" {
    void* memcpy(void* dest, const void* src, std::size_t n);
}