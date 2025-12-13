#pragma once
#include <cstdint>

namespace feron::serial {

    // I/O port helpers (use same outb convention as tty)
    static inline __attribute__((no_caller_saved_registers))
    void outb(uint16_t port, uint8_t val) {
        asm volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
    }

    static inline __attribute__((no_caller_saved_registers))
    uint8_t inb(uint16_t port) {
        uint8_t val;
        asm volatile ("inb %1, %0" : "=a"(val) : "Nd"(port));
        return val;
    }

    // Initialize COM1 (0x3F8) for 115200, 8N1
    inline __attribute__((no_caller_saved_registers))
    void init() {
        const uint16_t port = 0x3F8;
        outb(port + 1, 0x00);    // disable all interrupts
        outb(port + 3, 0x80);    // enable DLAB (set baud rate divisor)
        outb(port + 0, 0x01);    // divisor low byte (115200)
        outb(port + 1, 0x00);    // divisor high byte
        outb(port + 3, 0x03);    // 8 bits, no parity, one stop bit
        outb(port + 2, 0xC7);    // enable FIFO, clear them, with 14-byte threshold
        outb(port + 4, 0x0B);    // IRQs enabled, RTS/DSR set
    }

    inline __attribute__((no_caller_saved_registers))
    bool is_transmit_empty() {
        const uint16_t port = 0x3F8;
        return (inb(port + 5) & 0x20) != 0;
    }

    inline __attribute__((no_caller_saved_registers))
    void write_char(char c) {
        const uint16_t port = 0x3F8;
        // wait for transmitter ready
        while (!is_transmit_empty()) { asm volatile("pause"); }
        outb(port, static_cast<uint8_t>(c));
    }

    inline __attribute__((no_caller_saved_registers))
    void write(const char* s) {
        if (!s) return;
        while (*s) {
            if (*s == '\n') {
                write_char('\r'); // CRLF
            }
            write_char(*s++);
        }
    }
}
