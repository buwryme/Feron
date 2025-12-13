#pragma once

namespace feron::events {
    struct hour {
        using f = void(*)();

    private:
        f registered_fn = nullptr;

        // dummy function that does nothing
        static void noop() {}

    public:
        inline void register_fn(void* fn) {
            registered_fn = reinterpret_cast<f>(fn);
        }

        inline f get() {
            return registered_fn ? registered_fn : &noop;
        }
    } inline hour;
}
