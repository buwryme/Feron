#pragma once

#include "../classes/fstring.hpp"
#include "../serial.hpp"
#include <cstdint>

namespace feron::tty {
    constexpr int WIDTH  = 80;
    constexpr int HEIGHT = 25;
    inline volatile uint16_t* VGA = reinterpret_cast<volatile uint16_t*>(0xB8000);

    enum Color : uint8_t {
        BLACK = 0, BLUE = 1, GREEN = 2, CYAN = 3, RED = 4, MAGENTA = 5,
        BROWN = 6, LIGHT_GRAY = 7, DARK_GRAY = 8, LIGHT_BLUE = 9,
        LIGHT_GREEN = 10, LIGHT_CYAN = 11, LIGHT_RED = 12,
        LIGHT_MAGENTA = 13, YELLOW = 14, WHITE = 15
    };

    inline uint16_t make_cell(char c, Color fg = WHITE, Color bg = BLACK) {
        uint16_t attr = (static_cast<uint16_t>(bg) << 4) | static_cast<uint16_t>(fg);
        return static_cast<uint16_t>(static_cast<uint8_t>(c)) | (attr << 8);
    }

    // cursor state
    inline int cursor_row = 0;
    inline int cursor_col = 0;

    // hardware cursor control
    static inline void outb(uint16_t port, uint8_t val) {
        asm volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
    }

    inline void set_cursor(int x, int y) {
        if (x < 0) x = 0;
        if (y < 0) y = 0;
        cursor_col = x;
        cursor_row = y;
        int pos = y * WIDTH + x;
        outb(0x3D4, 0x0E);
        outb(0x3D5, (pos >> 8) & 0xFF);
        outb(0x3D4, 0x0F);
        outb(0x3D5, pos & 0xFF);
    }

    inline void scroll_up(Color fg = WHITE, Color bg = BLACK) {
        for (int r = 1; r < HEIGHT; ++r) {
            for (int c = 0; c < WIDTH; ++c) {
                VGA[(r - 1) * WIDTH + c] = VGA[r * WIDTH + c];
            }
        }
        uint16_t blank = make_cell(' ', fg, bg);
        for (int c = 0; c < WIDTH; ++c) VGA[(HEIGHT - 1) * WIDTH + c] = blank;
        if (cursor_row > 0) --cursor_row;
    }

    inline void clear(Color fg = WHITE, Color bg = BLACK) {
        uint16_t blank = make_cell(' ', fg, bg);
        for (int i = 0; i < WIDTH * HEIGHT; ++i)
            VGA[i] = blank;
        set_cursor(0, 0);
    }

    // low-level single char write to VGA
    inline void write_char(char c, Color fg = WHITE, Color bg = BLACK) {
        if (c == '\r') return;
        if (c == '\n') {
            cursor_row++;
            cursor_col = 0;
            if (cursor_row >= HEIGHT) scroll_up(fg, bg);
            set_cursor(cursor_col, cursor_row);
            return;
        }

        if (cursor_row < 0) cursor_row = 0;
        if (cursor_col < 0) cursor_col = 0;
        if (cursor_row >= HEIGHT) {
            scroll_up(fg, bg);
            cursor_row = HEIGHT - 1;
        }

        int pos = cursor_row * WIDTH + cursor_col;
        if (pos < 0) pos = 0;
        if (pos >= WIDTH * HEIGHT) {
            scroll_up(fg, bg);
            pos = (HEIGHT - 1) * WIDTH;
            cursor_row = HEIGHT - 1;
            cursor_col = 0;
        }

        VGA[pos] = make_cell(c, fg, bg);
        cursor_col++;
        if (cursor_col >= WIDTH) {
            cursor_col = 0;
            cursor_row++;
            if (cursor_row >= HEIGHT) scroll_up(fg, bg);
        }
        set_cursor(cursor_col, cursor_row);
    }

    // write C string to VGA and mirror to serial
    inline void write(const char* s, Color fg = WHITE, Color bg = BLACK) {
        if (!s) return;
        while (*s) {
            unsigned char ch = static_cast<unsigned char>(*s++);
            write_char(static_cast<char>(ch), fg, bg);
            serial::write_char(static_cast<char>(ch));
        }
    }

    inline void write(const feron::string& s, Color fg = WHITE, Color bg = BLACK) {
        write(s.c_str(), fg, bg);
    }

    inline void writeln(const char* s, Color fg = WHITE, Color bg = BLACK) {
        write(s, fg, bg);
        write_char('\n', fg, bg);
        serial::write_char('\n');
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

    // ASCII-safe write helpers
    inline void write_ascii(const char* s, Color fg = WHITE, Color bg = BLACK) {
        if (!s) return;
        while (*s) {
            unsigned char c = static_cast<unsigned char>(*s++);
            if (c < 0x20 || c > 0x7E) c = '?';
            write_char(static_cast<char>(c), fg, bg);
            serial::write_char(static_cast<char>(c));
        }
    }

    inline void write_asciiln(const char* s, Color fg = WHITE, Color bg = BLACK) {
        write_ascii(s, fg, bg);
        write_char('\n', fg, bg);
        serial::write_char('\n');
    }
}
