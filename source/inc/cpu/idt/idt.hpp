#pragma once

#include <cstdint>

namespace feron::cpu::idt {

    constexpr uint16_t KERNEL_CS = 0x08;
    constexpr uint8_t  IDT_INT_GATE = 0x8E; // present, DPL=0, 64-bit interrupt gate

    struct IDTEntry {
        uint16_t offset_low;
        uint16_t selector;
        uint8_t  ist;
        uint8_t  type_attr;
        uint16_t offset_mid;
        uint32_t offset_high;
        uint32_t zero;
    } __attribute__((packed));

    struct IDTPointer {
        uint16_t limit;
        uint64_t base;
    } __attribute__((packed));

    constexpr int IDT_SIZE = 256;
    inline IDTEntry idt[IDT_SIZE];
    inline IDTPointer idt_ptr;

    inline void set_idt_entry(int vector, void (*handler)(), uint16_t selector, uint8_t type_attr) {
        uint64_t addr = reinterpret_cast<uint64_t>(handler);
        idt[vector].offset_low  = addr & 0xFFFF;
        idt[vector].selector    = selector;
        idt[vector].ist         = 0;
        idt[vector].type_attr   = type_attr;
        idt[vector].offset_mid  = (addr >> 16) & 0xFFFF;
        idt[vector].offset_high = (addr >> 32) & 0xFFFFFFFF;
        idt[vector].zero        = 0;
    }

    inline void load_idt() {
        idt_ptr.limit = sizeof(idt) - 1;
        idt_ptr.base  = reinterpret_cast<uint64_t>(&idt[0]);
        asm volatile("lidt %0" : : "m"(idt_ptr));
    }

} // namespace feron::cpu
