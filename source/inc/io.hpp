#pragma once
#include <cstdint>
#include "serial.hpp"

namespace feron::io {

    // helper: print a byte as two hex chars
    inline void print_hex8(uint8_t v) {
        const char* hex = "0123456789ABCDEF";
        serial::write_char(hex[(v >> 4) & 0xF]);
        serial::write_char(hex[v & 0xF]);
    }

    // helper: print a 16-bit value as four hex chars
    inline void print_hex16(uint16_t v) {
        const char* hex = "0123456789ABCDEF";
        serial::write_char(hex[(v >> 12) & 0xF]);
        serial::write_char(hex[(v >> 8) & 0xF]);
        serial::write_char(hex[(v >> 4) & 0xF]);
        serial::write_char(hex[v & 0xF]);
    }

    inline void outb(uint16_t port, uint8_t val) {
        asm volatile ("outb %0, %1" : : "a"(val), "Nd"(port));

        // mirror to serial for debugging
        // serial::write_char('[');
        // serial::write_char('O');
        // serial::write_char('U');
        // serial::write_char('T');
        // serial::write_char(' ');
        // print_hex16(port);
        // serial::write_char('=');
        // print_hex8(val);
        // serial::write_char(']');
    }

    inline uint8_t inb(uint16_t port) {
        uint8_t val;
        asm volatile ("inb %1, %0" : "=a"(val) : "Nd"(port));

        // mirror to serial for debugging
        // serial::write_char('[');
        // serial::write_char('I');
        // serial::write_char('N');
        // serial::write_char(' ');
        // print_hex16(port);
        // serial::write_char('=');
        // print_hex8(val);
        // serial::write_char(']');
        return val;
    }
}
