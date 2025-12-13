#pragma once
#include <cstdint>

namespace feron::cpu::gdt {

    struct [[gnu::packed]] GDTPtr {
        uint16_t limit;
        uint64_t base;
    };

    // Minimal GDT: null, kernel code, kernel data
    alignas(8) static const uint64_t gdt_table[] = {
        0x0000000000000000ULL, // null descriptor (index 0)
        0x00AF9A000000FFFFULL, // kernel code descriptor (index 1 -> selector 0x08)
        0x00AF92000000FFFFULL  // kernel data descriptor (index 2 -> selector 0x10)
    };

    static GDTPtr gdt_ptr = {
        static_cast<uint16_t>(sizeof(gdt_table) - 1),
        reinterpret_cast<uint64_t>(gdt_table)
    };

    inline void load_gdt_cpp() {
        // Load GDT
        asm volatile("lgdt %0" : : "m"(gdt_ptr));

        // Reload data segment registers with selector 0x10 (index 2)
        asm volatile(
            "mov $0x10, %%ax\n\t"
            "mov %%ax, %%ds\n\t"
            "mov %%ax, %%es\n\t"
            "mov %%ax, %%ss\n\t"
            "mov %%ax, %%fs\n\t"
            "mov %%ax, %%gs\n\t"
            ::: "ax"
        );

        // Far jump to reload CS with selector 0x08 (index 1)
        asm volatile(
            "pushq $0x08\n\t"
            "lea 1f(%%rip), %%rax\n\t"
            "pushq %%rax\n\t"
            "lretq\n\t"
            "1:\n\t"
            ::: "rax"
        );
    }

} // namespace feron::cpu::gdt
