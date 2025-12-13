#pragma once

#include <cstdint>
#include <cstddef>

namespace feron::boot::mb2 {

    struct tag_t {
        uint32_t type;
        uint32_t size;
    };

    struct mmapEntry_t {
        uint64_t addr;
        uint64_t len;
        uint32_t type;    // 1 = available, others reserved
        uint32_t reserved;
    };

    struct module_t {
        const char* mod_start;
        const char* mod_end;
        const char* string; // module string (may be empty)
    };

    struct framebuffer_t {
        uintptr_t addr = 0;
        uint32_t width = 0;
        uint32_t height = 0;
        uint32_t pitch = 0;
        uint8_t bpp = 0;
        uint32_t type = 0; // framebuffer type
    };

    struct info_t {
        // simple C-style pointers (no heap allocation)
        const char* cmdline = nullptr;
        const char* bootloader = nullptr;

        // memory map
        const mmapEntry_t* mmap = nullptr;
        uint32_t mmap_count = 0;

        // modules
        const module_t* modules = nullptr;
        uint32_t modules_count = 0;

        // framebuffer (if present)
        framebuffer_t framebuffer{};
    };

    inline std::size_t align_up(std::size_t n, std::size_t a) {
        return (n + (a - 1)) & ~(a - 1);
    }

    inline info_t parse(void* mbi) {
        info_t info{};
        if (!mbi) return info;

        auto* base = reinterpret_cast<uint8_t*>(mbi);
        // first 4 bytes = total size
        uint32_t total_size = *reinterpret_cast<uint32_t*>(base);
        // safety: require at least header
        if (total_size < 8) return info;

        uint8_t* cur = base + 8; // skip size + reserved
        uint8_t* end_all = base + total_size;

        while (cur + sizeof(tag_t) <= end_all) {
            auto* tag = reinterpret_cast<tag_t*>(cur);
            if (tag->type == 0) break; // end tag

            // ensure tag->size is sane
            uint32_t tsize = tag->size;
            if (tsize < sizeof(tag_t)) break;
            uint8_t* tag_end = cur + tsize;
            if (tag_end > end_all) break;

            switch (tag->type) {
                case 1: { // cmdline
                    const char* s = reinterpret_cast<const char*>(cur + sizeof(tag_t));
                    if (s && *s) info.cmdline = s;
                    break;
                }
                case 2: { // bootloader name
                    const char* s = reinterpret_cast<const char*>(cur + sizeof(tag_t));
                    if (s && *s) info.bootloader = s;
                    break;
                }
                case 3: { // module
                    // multiboot2 module tag layout: tag + mod_start (u32) + mod_end (u32) + string...
                    if (tsize >= sizeof(tag_t) + 8) {
                        uint32_t mod_start = *reinterpret_cast<uint32_t*>(cur + sizeof(tag_t) + 0);
                        uint32_t mod_end   = *reinterpret_cast<uint32_t*>(cur + sizeof(tag_t) + 4);
                        const char* s = reinterpret_cast<const char*>(cur + sizeof(tag_t) + 8);
                        // store module info in-place by using the tag area as module array
                        // We'll create a temporary module_t array by reinterpreting the tag area.
                        // To avoid heap usage, we will treat the modules pointer as pointing into the mbi region.
                        // Build a small local module_t on the fly: but we need to collect count and pointer.
                        // Simpler: treat the modules pointer as the first module tag location and compute count later.
                        // We'll store modules pointer as the first module tag address (cast to module_t*).
                        // Count will be computed by scanning all module tags.
                        // For safety, we only set modules pointer if not set yet.
                        if (!info.modules) {
                            info.modules = reinterpret_cast<const module_t*>(cur);
                        }
                        // increment modules_count by 1 (we'll interpret modules array carefully in callers)
                        info.modules_count++;
                    }
                    break;
                }
                case 6: { // memory map
                    // layout: tag + entry_size (u32) + entry_version (u32) + entries...
                    if (tsize >= sizeof(tag_t) + 8) {
                        uint32_t entry_size = *reinterpret_cast<uint32_t*>(cur + sizeof(tag_t) + 0);
                        uint8_t* entries = cur + sizeof(tag_t) + 8;
                        uint8_t* entries_end = tag_end;
                        if (entry_size >= sizeof(mmapEntry_t) && entries < entries_end) {
                            info.mmap = reinterpret_cast<const mmapEntry_t*>(entries);
                            info.mmap_count = static_cast<uint32_t>((entries_end - entries) / entry_size);
                        }
                    }
                    break;
                }
                case 8: { // framebuffer (framebuffer tag types vary; handle common layout)
                    // Multiboot2 framebuffer tag (type 8) layout varies; handle common fields:
                    // at offset 8: framebuffer_addr (u64), at 16: framebuffer_pitch (u32), 20: width (u32), 24: height (u32), 28: bpp (u8) etc.
                    if (tsize >= sizeof(tag_t) + 32) {
                        uint64_t fb_addr = *reinterpret_cast<uint64_t*>(cur + sizeof(tag_t) + 0);
                        uint32_t fb_pitch = *reinterpret_cast<uint32_t*>(cur + sizeof(tag_t) + 8);
                        uint32_t fb_width = *reinterpret_cast<uint32_t*>(cur + sizeof(tag_t) + 12);
                        uint32_t fb_height = *reinterpret_cast<uint32_t*>(cur + sizeof(tag_t) + 16);
                        uint8_t fb_bpp = *reinterpret_cast<uint8_t*>(cur + sizeof(tag_t) + 20);
                        info.framebuffer.addr = static_cast<uintptr_t>(fb_addr);
                        info.framebuffer.pitch = fb_pitch;
                        info.framebuffer.width = fb_width;
                        info.framebuffer.height = fb_height;
                        info.framebuffer.bpp = fb_bpp;
                        info.framebuffer.type = 1;
                    }
                    break;
                }
                default:
                    break;
            }

            cur += align_up(tsize, 8);
        }

        return info;
    }

} // namespace feron::boot::mb2
