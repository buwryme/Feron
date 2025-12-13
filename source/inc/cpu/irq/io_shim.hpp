#pragma once

#include <cstdint>
#include "../../serial.hpp"

namespace io {
    inline void outb(uint16_t port, uint8_t val) { feron::serial::outb(port, val); }
    inline uint8_t inb(uint16_t port) { return feron::serial::inb(port); }
}
