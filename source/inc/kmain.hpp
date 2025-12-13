#pragma once

#include "boot/mb2.hpp"
#include "tty/tty.hpp"
#include "runtime/malloc.hpp"
#include "serial.hpp"
#include <cstdint>

namespace feron {
    inline void kmain(uint32_t magic, void* mbi) {
        // init serial first so we get early logs on -serial stdio
        feron::serial::init();

        tty::clear(tty::LIGHT_GRAY, tty::BLACK);
        tty::write("feron booted !!!\n\n");

        auto info = feron::boot::mb2::parse(mbi);

        // Try to initialize heap from first available mmap region (type == 1)
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
                    break;
                }
            }
        } else {
            tty::write("no mmap available to init heap\n");
        }

        // Small heap self-test: allocate, write, free
        void* a = nullptr;
        void* b = nullptr;
        a = malloc(64);
        tty::write("malloc(64) -> ");
        tty::write_hex64(reinterpret_cast<uint64_t>(a));
        tty::write("\n");

        b = malloc(128);
        tty::write("malloc(128) -> ");
        tty::write_hex64(reinterpret_cast<uint64_t>(b));
        tty::write("\n");

        if (a) {
            free(a);
            tty::write("free(a) done\n");
        }
        if (b) {
            free(b);
            tty::write("free(b) done\n");
        }

        // Print bootloader and cmdline if present
        if (info.bootloader) {
            tty::write("found bootloader: \"");
            tty::write(info.bootloader);
            tty::write("\"\n");
        } else {
            tty::write("no bootloader detected.\n");
        }

        if (info.cmdline) {
            tty::write("found cmdline: \"");
            tty::write(info.cmdline);
            tty::write("\"\n");
        } else {
            tty::write("no cmdline detected.\n");
        }

        if (info.mmap && info.mmap_count > 0) {
            auto& m = info.mmap[0];
            tty::write("\nfirst mmap entry:\n");
            tty::write(" addr:");
            tty::write_hex64(m.addr);
            tty::write("\n");
            tty::write(" len:");
            tty::write_hex64(m.len);
            tty::write("\n");
            tty::write(" type: ");
            char type_char[2] = { static_cast<char>('0' + (m.type & 0xF)), '\0' };
            tty::write(type_char);
            tty::write("\n");
        }

        // keep running (or return to caller if your entry expects that)
        for (;;) { asm volatile("hlt"); }
    }
}
