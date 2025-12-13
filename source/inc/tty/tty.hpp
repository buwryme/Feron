#pragma once

#include "../classes/fstring.hpp"
#include "../serial.hpp"
#include <cstdint>

namespace feron::tty {
    constexpr int WIDTH  = 80;
    constexpr int HEIGHT = 25;
    inline uint16_t* VGA = reinterpret_cast<uint16_t*>(0xB8000);

    enum Color : uint8_t {
        BLACK = 0, BLUE = 1, GREEN = 2, CYAN = 3, RED = 4, MAGENTA = 5,
        BROWN = 6, LIGHT_GRAY = 7, DARK_GRAY = 8, LIGHT_BLUE = 9,
        LIGHT_GREEN = 10, LIGHT_CYAN = 11, LIGHT_RED = 12,
        LIGHT_MAGENTA = 13, YELLOW = 14, WHITE = 15
    };

    inline uint16_t make_cell(char c, Color fg = WHITE, Color bg = BLACK) {
        return (uint16_t)c | ((uint16_t)(fg | (bg << 4)) << 8);
    }

    // cursor state
    inline int cursor_row = 0;
    inline int cursor_col = 0;

    // hardware cursor control
    static inline void outb(uint16_t port, uint8_t val) {
        asm volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
    }

    inline void set_cursor(int x, int y) {
        cursor_col = x;
        cursor_row = y;
        int pos = y * WIDTH + x;
        outb(0x3D4, 0x0E);
        outb(0x3D5, (pos >> 8) & 0xFF);
        outb(0x3D4, 0x0F);
        outb(0x3D5, pos & 0xFF);
    }

    inline void clear(Color fg = WHITE, Color bg = BLACK) {
        uint16_t blank = make_cell(' ', fg, bg);
        for (int i = 0; i < WIDTH * HEIGHT; ++i)
            VGA[i] = blank;
        set_cursor(0, 0);
    }

    // auto-managed write: uses current cursor_row/col, advances automatically
    inline void write(const char* s, Color fg = WHITE, Color bg = BLACK) {
        if (!s) return;
        int pos = cursor_row * WIDTH + cursor_col;
        while (*s) {
            char ch = *s++;
            // send to serial as well
            serial::write_char(ch);

            if (ch == '\n') {
                cursor_row++;
                cursor_col = 0;
                pos = cursor_row * WIDTH;
                continue;
            }
            if (pos >= WIDTH * HEIGHT) break;
            VGA[pos++] = make_cell(ch, fg, bg);
            cursor_col++;
            if (cursor_col >= WIDTH) {
                cursor_col = 0;
                cursor_row++;
                pos = cursor_row * WIDTH;
            }
        }
        set_cursor(cursor_col, cursor_row);
    }

    inline void write_hex64(uint64_t val, Color fg = WHITE, Color bg = BLACK) {
        char buf[17];
        const char* hexchars = "0123456789ABCDEF";
        for (int i = 0; i < 16; ++i) {
            int shift = (15 - i) * 4;
            buf[i] = hexchars[(val >> shift) & 0xF];
        }
        buf[16] = '\0';
        write(buf, fg, bg);
    }

    inline void write(const feron::string& s, Color fg = WHITE, Color bg = BLACK) {
        write(s.c_str(), fg, bg);
    }

    // convenience: write C string and newline
    inline void writeln(const char* s, Color fg = WHITE, Color bg = BLACK) {
        write(s, fg, bg);
        write("\n", fg, bg);
    }
}
