#pragma once

#include "boot/mb2.hpp"
#include "cpu/idt/handlers.hpp"
#include "cpu/irq/toggler.hpp"
#include "cpu/irq/irq.hpp"
#include "cpu/irq/pic.hpp"
#include "cpu/irq/pit.hpp"
#include "tty/tty.hpp"
#include "runtime/heap_init.hpp"
#include "serial.hpp"
#include "cpu/gdt.hpp"        // GDT loader (Option B)
#include "cpu/idt/idt.hpp"    // IDT setup + register_exceptions
#include <cstdint>

int uptime = 0;

inline void my_tick() {
    // dummy for less on-screen garbage
    // feron::tty::write("meow!!!");
}

inline void my_second() {
    uptime++;
    feron::tty::write("second passed... uptime = ");
    feron::tty::write_dec(uptime);   // prints as hex
    feron::tty::write("\n");
}

namespace feron {
    inline void kmain(uint32_t magic, void* mbi) {
        // Early init
        serial::init();
        tty::clear(tty::LIGHT_GRAY, tty::BLACK);
        tty::writeln("feron booted !!!");

        // Parse multiboot info and init heap
        auto info = feron::boot::mb2::parse(mbi);
        feron::runtime::init_heap_from_mmap(info);

        // Install GDT so selector 0x08 is valid
        feron::cpu::gdt::load_gdt_cpp();
        tty::writeln("GDT loaded.");

        // Register and load IDT
        feron::cpu::idt::handlers::register_exceptions();
        feron::cpu::idt::load_idt();
        tty::writeln("IDT registered and loaded.");

        feron::events::second.register_fn(reinterpret_cast<void*>(&my_second));

        cpu::irq::pic::pic_remap(0x20, 0x28);
        cpu::irq::pic::pic_unmask(0); // IRQ0: PIT
        cpu::irq::pic::pic_unmask(1); // IRQ1: keyboard
        tty::writeln("PIC remapped, IRQ0/IRQ1 unmasked.");

        feron::cpu::irq::register_irqs();

        pit_set_frequency(60);

        enable_interrupts();

        return;
    }
}
