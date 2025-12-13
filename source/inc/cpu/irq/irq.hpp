#pragma once

#include <cstdint>
#include "../idt/idt.hpp"
#include "../irq/pic.hpp"
#include "../../tty/tty.hpp"
#include "../../events/tick.hpp"
#include "../../events/second.hpp"
#include "../../events/minute.hpp"
#include "../../events/hour.hpp"
#include "keyboard.hpp"

// IRQ vectors after remap: 32..47
constexpr int IRQ_BASE = 0x20;

// Simple ISR function signatures
namespace feron::cpu::irq {
    struct [[gnu::packed]] InterruptFrame {
        uint64_t rip, cs, rflags, rsp, ss;
    };

    // Timer (IRQ0)
    extern "C" inline __attribute__((interrupt))
    void isr_irq0(InterruptFrame* frame) {
        static uint64_t ticks = 0;
        ++ticks;

        feron::events::tick.get()();

        if (ticks % 60 == 0) feron::events::second.get()();
        if (ticks % (60 * 60) == 0) feron::events::minute.get()();
        if (ticks % (60 * 60 * 60) == 0) feron::events::hour.get()();

        pic::pic_eoi(0);
    }

    extern "C" inline __attribute__((interrupt))
    void isr_irq1(InterruptFrame* frame) {
        uint8_t sc = io::inb(0x60);
        char c = keyboard::translate(sc);
        if (c) {
            tty::write_char(c);
            serial::write_char(c);
        }
        pic::pic_eoi(1);
    }

    inline void register_irqs() {
        using feron::cpu::idt::set_idt_entry;
        // type_attr: 0x8E = present, DPL=0, interrupt gate
        set_idt_entry(IRQ_BASE + 0, reinterpret_cast<void(*)()>(&isr_irq0), 0x08, 0x8E);
        set_idt_entry(IRQ_BASE + 1, reinterpret_cast<void(*)()>(&isr_irq1), 0x08, 0x8E);
        // You can add more IRQs 2..15 as needed.
    }
}
