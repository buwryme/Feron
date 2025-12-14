#pragma once

#include "../boot/mb2.hpp"
#include "impl/mm/kernel_heap_init.hpp"
#include "../tty/tty.hpp"
#include <cstdint>

namespace feron::runtime {

    inline void init_heap_from_mmap(const feron::boot::mb2::info_t& info) {
        if (info.mmap && info.mmap_count > 0) {
            for (uint32_t i = 0; i < info.mmap_count; ++i) {
                auto& e = info.mmap[i];
                if (e.type == 1 && e.len > 0) {
                    // Clamp to first 4 MiB identity window
                    uintptr_t start = static_cast<uintptr_t>(e.addr + 0x1000);
                    uintptr_t end   = static_cast<uintptr_t>(e.addr + e.len);
                    if (start >= 0x00400000) continue; // skip regions starting above 4 MiB
                    if (end   >  0x00400000) end = 0x00400000;

                    std::size_t heap_size = (end > start) ? static_cast<std::size_t>(end - start) : 0;
                    if (heap_size < 64) continue;
                    if (heap_size > 0x100000) heap_size = 0x100000; // keep bootstrap heap small (e.g., 1 MiB)

                    void* heap_addr = reinterpret_cast<void*>(start);
                    kernel_heap_init(heap_addr, heap_size);
                    return;
                }
            }
        }
    }
}
