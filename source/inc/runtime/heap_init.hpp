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
                    // choose a safe offset inside the region (skip first page)
                    void* heap_addr = reinterpret_cast<void*>(static_cast<uintptr_t>(e.addr + 0x1000));
                    std::size_t heap_size = static_cast<std::size_t>(e.len - 0x1000);
                    // clamp to something reasonable if needed
                    if (heap_size > 0x10000000) heap_size = 0x10000000; // 256MB cap
                    kernel_heap_init(heap_addr, heap_size);
                    tty::write("kernel heap initialized\n");
                    return;
                }
            }
        }
        tty::write("no mmap available to init heap\n");
    }

}
