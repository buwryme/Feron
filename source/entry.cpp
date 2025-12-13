#include "inc/kmain.hpp"

extern "C" void kernel_main(uint32_t magic, void* mbi) {
    // do the assigned functions
    feron::kmain(magic, mbi);

    // halt
    for (;;) asm volatile("hlt");
}
