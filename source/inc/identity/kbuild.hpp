#pragma once

#ifndef BUILD_COMPILER
#define BUILD_COMPILER "unknown"
#endif

#ifndef BUILD_OS
#define BUILD_OS "unknown"
#endif

#ifndef BUILD_HOST
#define BUILD_HOST "unknown"
#endif

#define BUILD_DATE __DATE__
#define BUILD_TIME __TIME__

namespace feron::identity::kbuild {

    struct kbuild_info_t {
        const char* compiler;
        const char* os;
        const char* host;
        const char* date;
        const char* time;
    };

    inline kbuild_info_t get() {
        return {
            BUILD_COMPILER,
            BUILD_OS,
            BUILD_HOST,
            BUILD_DATE,
            BUILD_TIME
        };
    }

} // namespace feron::identity::kbuild
