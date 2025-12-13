#pragma once

#include <cstddef>

extern "C" {
    void* realloc(void* ptr, std::size_t newsize);
}