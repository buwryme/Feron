#pragma once

#include <cstdint>
#include "../idt/idt.hpp"
#include "../irq/pic.hpp"
#include "../irq/io_shim.hpp"   // for io::inb/outb helpers
#include "../../serial.hpp"
#include "../../tty/tty.hpp"
#include "../../events/tick.hpp"
#include "../../events/second.hpp"
#include "../../events/minute.hpp"
#include "../../events/hour.hpp"
#include "keyboard.hpp"

// IRQ vectors after PIC remap: 32..47
constexpr int IRQ_BASE = 0x20;

namespace feron::cpu::irq {

    // Minimal interrupt frame pushed by hardware + compiler attribute
    struct [[gnu::packed]] InterruptFrame {
        uint64_t rip;
        uint64_t cs;
        uint64_t rflags;
        uint64_t rsp;
        uint64_t ss;
    };

    // --- IRQ0: PIT timer ---
    extern "C" inline __attribute__((interrupt))
    void isr_irq0(InterruptFrame* /*frame*/) {
        static uint64_t ticks = 0;
        ++ticks;

        // Dispatch tick event
        feron::events::tick.get()();

        // Dispatch higher‑level events
        if (ticks % 60 == 0) feron::events::second.get()();
        if (ticks % (60 * 60) == 0) feron::events::minute.get()();
        if (ticks % (60 * 60 * 60) == 0) feron::events::hour.get()();

        // End of interrupt
        pic::pic_eoi(0);
    }

    // --- IRQ1: Keyboard ---
    extern "C" inline __attribute__((interrupt))
    void isr_irq1(InterruptFrame* /*frame*/) {
        // Read scancode from port 0x60
        uint8_t sc = io::inb(0x60);

        // Push raw scancode into buffer
        feron::kbd::buf_push(sc);

        // Optionally translate immediately and echo
        char c;
        if (feron::kbd::getch(c)) {
            tty::write_char(c);
            serial::write_char(c);
            if (feron::kbd::on_key) feron::kbd::on_key(c);
        }

        // End of interrupt
        pic::pic_eoi(1);
    }

    // --- Registration into IDT ---
    inline void register_irqs() {
        using feron::cpu::idt::set_idt_entry;
        constexpr uint8_t type_attr = 0x8E; // present, DPL=0, interrupt gate

        set_idt_entry(IRQ_BASE + 0,
                      reinterpret_cast<void(*)()>(&isr_irq0),
                      0x08, type_attr);
        set_idt_entry(IRQ_BASE + 1,
                      reinterpret_cast<void(*)()>(&isr_irq1),
                      0x08, type_attr);

        // Add more IRQs (2..15) here as you implement them
    }
}
