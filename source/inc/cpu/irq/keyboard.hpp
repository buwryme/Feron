#pragma once
#include <cstdint>
#include "../../serial.hpp"

namespace feron::kbd {
    // Simple ring buffer
    constexpr std::size_t BUF_CAP = 256;
    inline volatile uint8_t sc_buf[BUF_CAP];
    inline volatile std::size_t head = 0, tail = 0;

    inline bool buf_push(uint8_t sc) {
        std::size_t next = (head + 1) % BUF_CAP;
        if (next == tail) return false; // full
        sc_buf[head] = sc;
        head = next;
        return true;
    }
    inline bool buf_pop(uint8_t& sc) {
        if (tail == head) return false; // empty
        sc = sc_buf[tail];
        tail = (tail + 1) % BUF_CAP;
        return true;
    }

    // Modifiers
    inline bool shift = false, ctrl = false, alt = false, caps = false, ext = false;

    inline void update_modifiers(uint8_t sc) {
        if (sc == 0xE0) { ext = true; return; }
        bool break_code = (sc & 0x80) != 0;
        uint8_t code = sc & 0x7F;

        auto set = [&](bool& m, bool on){ m = on; };
        switch (code) {
            case 0x2A: case 0x36: set(shift, !break_code); break; // left/right shift
            case 0x1D: set(ctrl, !break_code); break;
            case 0x38: set(alt, !break_code); break;
            case 0x3A: if (!break_code) caps = !caps; break; // toggle capslock
            default: break;
        }
        ext = false;
    }

    // Lookup tables for set‑1 scancodes
    static const char unshift[128] = {
        0, 27, '1','2','3','4','5','6','7','8','9','0','-','=', '\b',
        '\t','q','w','e','r','t','y','u','i','o','p','[',']','\n', 0,
        'a','s','d','f','g','h','j','k','l',';','\'','`', 0, '\\',
        'z','x','c','v','b','n','m',',','.','/', 0, '*', 0, ' ',
        // fill rest with 0
    };

    static const char shifted[128] = {
        0, 27, '!','@','#','$','%','^','&','*','(',')','_','+', '\b',
        '\t','Q','W','E','R','T','Y','U','I','O','P','{','}','\n', 0,
        'A','S','D','F','G','H','J','K','L',':','"','~', 0, '|',
        'Z','X','C','V','B','N','M','<','>','?', 0, '*', 0, ' ',
        // fill rest with 0
    };

    inline char translate_set1(uint8_t sc) {
        if (sc == 0xE0 || sc == 0xE1) return 0;
        bool break_code = (sc & 0x80) != 0;
        if (break_code) return 0;

        uint8_t code = sc & 0x7F;
        if (code >= 128) return 0;

        char ch;
        // CapsLock only affects letters
        if (shift) {
            ch = shifted[code];
        } else {
            ch = unshift[code];
        }
        if (!ch) return 0;

        if (caps && ch >= 'a' && ch <= 'z') {
            ch = static_cast<char>(ch - 32);
        }
        return ch;
    }

    // Public API
    inline bool getch(char& out) {
        uint8_t sc;
        if (!buf_pop(sc)) return false;
        update_modifiers(sc);
        char ch = translate_set1(sc);
        if (!ch) return false;
        out = ch;
        return true;
    }

    inline std::size_t read_line(char* buf, std::size_t maxlen) {
        if (!buf || maxlen == 0) return 0;
        std::size_t n = 0;
        while (n < maxlen - 1) {
            char c;
            if (!getch(c)) break;
            if (c == '\n') { buf[n++] = c; break; }
            if (c == '\b') { if (n) --n; continue; }
            buf[n++] = c;
        }
        buf[n] = '\0';
        return n;
    }

    // Optional callback
    using on_key_t = void(*)(char c);
    inline on_key_t on_key = nullptr;
    inline void set_on_key(on_key_t cb) { on_key = cb; }
}
