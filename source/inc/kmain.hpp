#pragma once

#include "boot/mb2.hpp"
#include "tty/tty.hpp"
#include "runtime/heap_init.hpp"
#include "serial.hpp"
#include <cstdint>

namespace feron {
    inline void kmain(uint32_t magic, void* mbi) {
        // init serial first so we get early logs on -serial stdio
        feron::serial::init();

        tty::clear(tty::LIGHT_GRAY, tty::BLACK);
        tty::write("feron booted !!!\n\n");

        auto info = feron::boot::mb2::parse(mbi);

        feron::runtime::init_heap_from_mmap(info);

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
