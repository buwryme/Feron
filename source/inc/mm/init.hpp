#pragma once

#include "../runtime/heap_init.hpp"
#include "config.hpp"
#include "paging.hpp"
#include "pfa.hpp"
#include "valloc.hpp"
#include "../boot/mb2.hpp"

namespace feron::mm {
    inline void init(boot::mb2::info_t info) {
        feron::mm::pfa::init(info);

        feron::runtime::init_heap_from_mmap(info);

        feron::mm::valloc::init
        (
            feron::mm::config::va_pool_base,
            feron::mm::config::va_pool_size
        );

        feron::mm::paging::init
        (
            feron::mm::config::va_pool_base,
            feron::mm::config::va_pool_size,
            0,
            0,
            0,
            feron::mm::paging::P_PRESENT | feron::mm::paging::P_RW
        );
    }
}