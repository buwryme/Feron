#pragma once

#include <cstdint>
#include "../../io.hpp"
#include "keyboard.hpp"
#include "../../tty/tty.hpp"

namespace feron::kbd {
    constexpr uint16_t PS2_CMD = 0x64;
    constexpr uint16_t PS2_DATA = 0x60;

    inline void flush() {
        // Read any pending data
        for (int i = 0; i < 16; ++i) {
            uint8_t status = feron::io::inb(PS2_CMD);
            if ((status & 1) == 0) break;
            (void)feron::io::inb(PS2_DATA);
        }
    }

    inline void wait_input() {
        // Wait until input buffer is clear
        while (feron::io::inb(PS2_CMD) & 2) asm volatile("pause");
    }

    inline void wait_output() {
        // Wait until output buffer has data
        while ((feron::io::inb(PS2_CMD) & 1) == 0) asm volatile("pause");
    }

    inline void init() {
        flush();

        // Enable first PS/2 port (keyboard)
        wait_input();
        feron::io::outb(PS2_CMD, 0xAD); // disable port 1
        wait_input();
        feron::io::outb(PS2_CMD, 0xA7); // disable port 2 (if dual)

        // Enable keyboard scanning
        wait_input();
        feron::io::outb(PS2_CMD, 0x20); // read config byte
        wait_output();
        uint8_t cfg = feron::io::inb(PS2_DATA);
        cfg |= 0x01;  // enable IRQ1
        cfg &= ~0x10; // disable translation if needed
        wait_input();
        feron::io::outb(PS2_CMD, 0x60); // write config
        wait_input();
        feron::io::outb(PS2_DATA, cfg);

        // Enable keyboard device
        wait_input();
        feron::io::outb(PS2_CMD, 0xAE); // enable port 1

        // Send Enable Scanning (0xF4)
        wait_input();
        feron::io::outb(PS2_DATA, 0xF4);
        wait_output();
        (void)feron::io::inb(PS2_DATA); // ACK (0xFA)
    }
    
    inline void poll_once() {
        char c;
        if (getch(c)) {
            if (on_key) on_key(c);
            feron::tty::write_char(c);
            feron::serial::write_char(c);
        }
    }
}
