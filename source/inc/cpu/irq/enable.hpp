#pragma once

namespace feron {
    inline void enable_interrupts() { asm volatile("sti"); }
    inline void disable_interrupts() { asm volatile("cli"); }
}