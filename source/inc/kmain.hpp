#pragma once

#include "boot/mb2.hpp"
#include "cpu/idt/handlers.hpp"
#include "cpu/init.hpp"
#include "cpu/irq/toggler.hpp"
#include "cpu/irq/irq.hpp"
#include "cpu/irq/pic.hpp"
#include "cpu/irq/pit.hpp"
#include "mm/init.hpp"
#include "tty/tty.hpp"
#include "runtime/heap_init.hpp"
#include "serial.hpp"
#include "cpu/gdt.hpp"
#include "cpu/idt/idt.hpp"
#include <cstdint>
#include "identity/kbuild.hpp"

inline int uptime = 0;

inline void my_tick() {
    // Keep output minimal to avoid flooding
    // feron::tty::write("meow!!!");
}

inline void trigger_pf_unmap_then_touch() {
    uint64_t pa = feron::mm::pfa::alloc_page();
    if (!pa) { feron::tty::writeln("PF test: alloc_page failed"); return; }

    uint64_t va = feron::mm::valloc::alloc_range(feron::mm::pfa::PAGE_SIZE);
    if (!va) { feron::tty::writeln("PF test: alloc_range failed"); return; }

    if (!feron::mm::paging::map_page(va, pa, feron::mm::paging::P_PRESENT | feron::mm::paging::P_RW)) {
        feron::tty::writeln("PF test: map_page failed"); return;
    }

    // Unmap the leaf to force a #PF on touch
    if (auto leaf = feron::mm::paging::walk_create(va)) {
        *leaf = 0;
        feron::mm::paging::invlpg(va);
    } else {
        feron::tty::writeln("PF test: walk_create failed"); return;
    }

    volatile uint8_t* bad = reinterpret_cast<volatile uint8_t*>(va);
    *bad = 0x42; // will fault
}

inline void my_second() {
    ++uptime;
    feron::tty::write("second passed... uptime = ");
    feron::tty::write_dec(uptime);
    feron::tty::write("\n");

    // if (uptime == 5) { trigger_pf_unmap_then_touch(); }
}

inline void my_minute() {
    feron::tty::writeln("minute passed...");
}

namespace feron {
    inline void kmain(uint32_t /*magic*/, void* mbi) {
        serial::init();
        tty::clear(tty::LIGHT_GRAY, tty::BLACK);
        tty::writeln("feron booted !!!");

        // Parse multiboot info
        auto info = feron::boot::mb2::parse(mbi);

        if (info.bootloader) {
            tty::write("bootloader: \""); tty::write(info.bootloader); tty::writeln("\"");
        }
        if (info.cmdline) {
            tty::write("cmdline: \""); tty::write(info.cmdline); tty::writeln("\"");
        }

        auto binfo = identity::kbuild::get();
        tty::writeln("build info:");
        tty::write("  compiler: "); tty::writeln(binfo.compiler);
        tty::write("  os: "); tty::writeln(binfo.os);
        tty::write("  host: "); tty::writeln(binfo.host);
        tty::write("  when: "); tty::write_ascii(binfo.date); tty::write(", "); tty::write_asciiln(binfo.time);

        // Memory init (includes heap init from mmap if available)
        mm::init(info);
        tty::writeln("memory subsystems initialized;");

        // CPU + IDT + PIC
        cpu::init();
        tty::writeln("cpu subsystems initialized;");

        feron::events::tick.register_fn(reinterpret_cast<void*>(my_tick));
        feron::events::second.register_fn(reinterpret_cast<void*>(my_second));
        feron::events::minute.register_fn(reinterpret_cast<void*>(my_minute));

        // Register IRQ handlers and PIT
        feron::cpu::irq::register_irqs();
        cpu::irq::pit::pit_set_frequency(60);

        enable_interrupts();

        // Test a mapping (optional)
        if (uint64_t pa = feron::mm::pfa::alloc_page()) {
            if (uint64_t va = feron::mm::valloc::alloc_range(feron::mm::pfa::PAGE_SIZE)) {
                if (feron::mm::paging::map_page(va, pa, feron::mm::paging::P_PRESENT | feron::mm::paging::P_RW)) {
                    tty::writeln("Mapped one test page dynamically.");
                }
            }
        }

        // Do not loop here; entry.cpp provides the idle HLT loop after return.
        return;
    }
}
