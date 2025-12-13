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
#include "mm/init.hpp"
#include <cstdint>

inline int uptime = 0;

inline void my_tick() {
    // dummy for less on-screen garbage
    // feron::tty::write("meow!!!");
}

inline void my_second() {
    uptime++;
    feron::tty::write("second passed... uptime = ");
    feron::tty::write_dec(uptime);
    feron::tty::write("\n");
}

inline void my_minute() {
    feron::tty::writeln("minute passed...");
}

namespace feron {
    inline void kmain(uint32_t magic, void* mbi) {
        serial::init();
        tty::clear(tty::LIGHT_GRAY, tty::BLACK);
        tty::writeln("feron booted !!!");

        // Parse multiboot info and init heap
        auto info = feron::boot::mb2::parse(mbi);
        feron::runtime::init_heap_from_mmap(info);

        // Initialize memory management
        mm::init(info);
        tty::writeln("paging subsystem initialized;");

        cpu::init();
        tty::writeln("cpu subsystems initialized;");

        // Register event callbacks
        feron::events::second.register_fn(reinterpret_cast<void*>(&my_second));
        feron::events::minute.register_fn(reinterpret_cast<void*>(&my_minute));

        feron::cpu::irq::register_irqs();

        pit_set_frequency(60);

        enable_interrupts();

        // Test mapping (optional sanity check)
        uint64_t pa = feron::mm::pfa::alloc_page();
        if (pa) {
            uint64_t va = feron::mm::valloc::alloc_range(feron::mm::pfa::PAGE_SIZE);
            if (va) {
                feron::mm::paging::map_page(va, pa,
                    feron::mm::paging::P_PRESENT | feron::mm::paging::P_RW);
                tty::writeln("Mapped one test page dynamically.");
            }
        }

        return;
    }
}
