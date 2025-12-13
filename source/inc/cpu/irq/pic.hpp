#pragma once

#include <cstdint>
#include "../../io.hpp"

namespace feron::cpu::irq::pic {
    // PIC ports
    constexpr uint16_t PIC1_CMD = 0x20;
    constexpr uint16_t PIC1_DATA = 0x21;
    constexpr uint16_t PIC2_CMD = 0xA0;
    constexpr uint16_t PIC2_DATA = 0xA1;
    constexpr uint8_t PIC_EOI = 0x20;

    // Initialization control words
    constexpr uint8_t ICW1_INIT = 0x10;
    constexpr uint8_t ICW1_ICW4 = 0x01;
    constexpr uint8_t ICW4_8086 = 0x01;

    // Remap master to 0x20, slave to 0x28
    inline void pic_remap(uint8_t offset1 = 0x20, uint8_t offset2 = 0x28) {
        uint8_t a1 = io::inb(PIC1_DATA);
        uint8_t a2 = io::inb(PIC2_DATA);

        io::outb(PIC1_CMD, ICW1_INIT | ICW1_ICW4);
        io::outb(PIC2_CMD, ICW1_INIT | ICW1_ICW4);

        io::outb(PIC1_DATA, offset1);
        io::outb(PIC2_DATA, offset2);

        io::outb(PIC1_DATA, 0x04); // tell master there is a slave at IRQ2
        io::outb(PIC2_DATA, 0x02); // tell slave its cascade identity

        io::outb(PIC1_DATA, ICW4_8086);
        io::outb(PIC2_DATA, ICW4_8086);

        // restore masks
        io::outb(PIC1_DATA, a1);
        io::outb(PIC2_DATA, a2);
    }

    inline void pic_set_mask(uint8_t master_mask, uint8_t slave_mask) {
        io::outb(PIC1_DATA, master_mask);
        io::outb(PIC2_DATA, slave_mask);
    }

    // Unmask specific IRQ line n (0..15)
    inline void pic_unmask(uint8_t irq) {
        if (irq < 8) {
            uint8_t m = io::inb(PIC1_DATA);
            m &= static_cast<uint8_t>(~(1u << irq));
            io::outb(PIC1_DATA, m);
        } else {
            irq -= 8;
            uint8_t m = io::inb(PIC2_DATA);
            m &= static_cast<uint8_t>(~(1u << irq));
            io::outb(PIC2_DATA, m);
        }
    }

    // End of interrupt
    inline void pic_eoi(uint8_t irq) {
        if (irq >= 8) io::outb(PIC2_CMD, PIC_EOI);
        io::outb(PIC1_CMD, PIC_EOI);
    }

}