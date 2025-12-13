#pragma once

#include "gdt.hpp"
#include "idt/handlers.hpp"
#include "idt/idt.hpp"
#include "irq/irq.hpp"

namespace feron::cpu {
    inline void init() {
        // Install GDT so selector 0x08 is valid
        feron::cpu::gdt::load_gdt_cpp();

        // Register and load IDT
        feron::cpu::idt::handlers::register_exceptions();
        feron::cpu::idt::load_idt();

        // PIC + IRQ setup
        cpu::irq::pic::pic_remap(0x20, 0x28);
        cpu::irq::pic::pic_unmask(0); // IRQ0: PIT
        cpu::irq::pic::pic_unmask(1); // IRQ1: keyboard
    }
}