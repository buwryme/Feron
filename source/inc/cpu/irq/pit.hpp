#pragma once
#include <cstdint>
#include "../../io.hpp"

namespace feron::cpu::irq::pit {
    // PIT ports
    constexpr uint16_t PIT_CMD = 0x43;
    constexpr uint16_t PIT_CH0 = 0x40;

    // Set PIT channel 0 to frequency Hz (e.g., 60)
    inline void pit_set_frequency(uint32_t hz) {
        if (hz == 0) return;
        uint32_t divisor = 1193182u / hz;
        feron::io::outb(PIT_CMD, 0x36); // channel 0, lo/hi, mode 3, binary
        feron::io::outb(PIT_CH0, static_cast<uint8_t>(divisor & 0xFF));       // low byte
        feron::io::outb(PIT_CH0, static_cast<uint8_t>((divisor >> 8) & 0xFF)); // high byte
    }
}