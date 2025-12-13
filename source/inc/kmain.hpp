#pragma once

#include "boot/mb2.hpp"
#include "cpu/idt/handlers.hpp"
#include "cpu/irq/enable.hpp"
#include "cpu/irq/irq.hpp"
#include "cpu/irq/pic.hpp"
#include "cpu/irq/pit.hpp"
#include "tty/tty.hpp"
#include "runtime/heap_init.hpp"
#include "serial.hpp"
#include "cpu/gdt.hpp"        // GDT loader (Option B)
#include "cpu/idt/idt.hpp"    // IDT setup + register_exceptions
#include <cstdint>

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

        cpu::irq::pic::pic_remap(0x20, 0x28);
        cpu::irq::pic::pic_unmask(0); // IRQ0: PIT
        cpu::irq::pic::pic_unmask(1); // IRQ1: keyboard
        tty::writeln("PIC remapped, IRQ0/IRQ1 unmasked.");

        feron::cpu::irq::register_irqs();

        pit_set_frequency(60);

        enable_interrupts();

        // Guaranteed #UD: Invalid Opcode (ISR 6) -- WORKING
        // tty::writeln("Triggering invalid opcode (ISR 6)...");
        // asm volatile("ud2");

        // Guaranteed #DE: Divide Error (ISR 0) -- WORKING
        // tty::writeln("Triggering divide-by-zero (ISR 0)...");
        // asm volatile(
        //     "mov $1, %%rax\n\t"
        //     "xor %%rdx, %%rdx\n\t"
        //     "xor %%rcx, %%rcx\n\t"
        //     "idiv %%rcx\n\t"
        //     :
        //     :
        //     : "rax", "rdx", "rcx"
        // );

        // Guaranteed #PF: Page Fault (ISR 14) -- WORKING
        // tty::writeln("Triggering page fault (ISR 14)...");
        // volatile uint64_t* bad = reinterpret_cast<volatile uint64_t*>(0x00400000); // 4 MiB
        // *bad = 0xDEADBEEF;

        // Halt if handlers return (they shouldn't)
        for (;;) asm volatile("hlt");
    }
}
