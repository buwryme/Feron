// source/inc/cpu/irq/keyboard.hpp
#pragma once
#include <cstdint>

namespace keyboard {
    static const char scancode_set1[128] = {
        0,  27, '1','2','3','4','5','6','7','8','9','0','-','=', '\b',
        '\t','q','w','e','r','t','y','u','i','o','p','[',']','\n', 0,
        'a','s','d','f','g','h','j','k','l',';','\'','`', 0, '\\',
        'z','x','c','v','b','n','m',',','.','/', 0, '*', 0, ' ',
        // rest can be filled as needed
    };

    inline char translate(uint8_t scancode) {
        if (scancode < sizeof(scancode_set1))
            return scancode_set1[scancode];
        return 0;
    }
}
