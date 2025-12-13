#include "inc/kmain.hpp"

extern "C" void kernel_main(uint32_t magic, void* mbi) {
    feron::kmain(magic, mbi);
}
