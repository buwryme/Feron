#pragma once
#include <cstdint>
#include "../../tty/tty.hpp"
#include "settings.hpp"
#include "idt.hpp"

namespace feron::cpu::idt::handlers {

    struct interrupt_frame {
        uint64_t rip;
        uint64_t cs;
        uint64_t flags;
        uint64_t rsp;
        uint64_t ss;
    };

    // ASCII-only names
    static constexpr const char* EXNAMES[32] = {
        "#DE Divide Error",              // 0
        "#DB Debug",                     // 1
        "NMI",                           // 2
        "#BP Breakpoint",                // 3
        "#OF Overflow",                  // 4
        "#BR Bound Range Exceeded",      // 5
        "#UD Invalid Opcode",            // 6
        "#NM Device Not Available",      // 7
        "#DF Double Fault",              // 8
        "Coprocessor Segment Overrun",   // 9
        "#TS Invalid TSS",               // 10
        "#NP Segment Not Present",       // 11
        "#SS Stack Segment Fault",       // 12
        "#GP General Protection Fault",  // 13
        "#PF Page Fault",                // 14
        "Reserved",                      // 15
        "#MF x87 Floating-Point Error",  // 16
        "#AC Alignment Check",           // 17
        "#MC Machine Check",             // 18
        "#XM SIMD Floating-Point",       // 19
        "#VE Virtualization Exception",  // 20
        "Reserved","Reserved","Reserved","Reserved",
        "Reserved","Reserved","Reserved","Reserved",
        "Reserved","Reserved","Reserved"
    };

    inline void print_kv_hex(const char* key, uint64_t val) {
        tty::write_ascii(key);
        tty::write_ascii(": ");
        tty::write_hex64(val);
        tty::write_asciiln("");
    }

    inline void render_frame(const interrupt_frame* f) {
        if (!f) return;
        print_kv_hex("RIP",    f->rip);
        print_kv_hex("CS",     f->cs);
        print_kv_hex("RFLAGS", f->flags);
        print_kv_hex("RSP",    f->rsp);
        print_kv_hex("SS",     f->ss);
    }

    inline void render_pf_error(uint64_t ec) {
        tty::write_ascii("Error code: ");
        tty::write_hex64(ec);
        tty::write_asciiln("");

        tty::write_ascii("  ");
        tty::write_hex64(ec);
        tty::write_ascii(" : ");

        bool first = true;
        auto add = [&](const char* s){
            if (!first) tty::write_ascii(", ");
            tty::write_ascii(s);
            first = false;
        };

        add((ec & (1ull<<0)) ? "P=protection" : "P=non-present");
        add((ec & (1ull<<1)) ? "W=write" : "R=read");
        add((ec & (1ull<<2)) ? "U=user" : "S=supervisor");
        if (ec & (1ull<<3)) add("RSVD");
        if (ec & (1ull<<4)) add("IF=instr-fetch");
        if (ec & (1ull<<5)) add("PK");
        if (ec & (1ull<<6)) add("SS");
        if (ec & (1ull<<7)) add("HLAT");
        tty::write_asciiln("");
    }

    inline void render_banner(const char* name) {
        if (::idt::clear_tty_on_crash) tty::clear(tty::LIGHT_GRAY, tty::BLACK);
        tty::set_cursor(0, 0);
        tty::write_ascii("=== CPU EXCEPTION ===");
        tty::write_asciiln("");
        tty::write_ascii("CPU exception encountered: ");
        tty::write_ascii(name);
        tty::write_asciiln("");
        tty::write_ascii("---------------------");
        tty::write_asciiln("");
    }

    extern "C" inline __attribute__((interrupt))
    void exception_handler(interrupt_frame* frame, uint64_t error_code) {
        render_banner("Generic exception (with error)");
        render_frame(frame);
        print_kv_hex("Error", error_code);
        for (;;) asm volatile("hlt");
    }

    extern "C" inline __attribute__((interrupt))
    void exception_handler_noerr(interrupt_frame* frame) {
        render_banner("Generic exception (no error)");
        render_frame(frame);
        for (;;) asm volatile("hlt");
    }

    extern "C" inline __attribute__((interrupt))
    void isr_divide_by_zero(interrupt_frame* frame) {
        render_banner(EXNAMES[0]);
        render_frame(frame);
        for (;;) asm volatile("hlt");
    }

    extern "C" inline __attribute__((interrupt))
    void isr_invalid_opcode(interrupt_frame* frame) {
        render_banner(EXNAMES[6]);
        render_frame(frame);
        for (;;) asm volatile("hlt");
    }

    extern "C" inline __attribute__((interrupt))
    void isr_page_fault(interrupt_frame* frame, uint64_t error_code) {
        uint64_t cr2 = 0; asm volatile("mov %%cr2, %0" : "=r"(cr2));
        render_banner(EXNAMES[14]);
        render_frame(frame);
        print_kv_hex("CR2 (fault addr)", cr2);
        render_pf_error(error_code);
        for (;;) asm volatile("hlt");
    }

    inline void register_exceptions() {
        feron::cpu::idt::set_idt_entry(0,  reinterpret_cast<void(*)()>(&isr_divide_by_zero), 0x08, 0x8E);
        feron::cpu::idt::set_idt_entry(1,  reinterpret_cast<void(*)()>(&exception_handler_noerr), 0x08, 0x8E);
        feron::cpu::idt::set_idt_entry(2,  reinterpret_cast<void(*)()>(&exception_handler_noerr), 0x08, 0x8E);
        feron::cpu::idt::set_idt_entry(3,  reinterpret_cast<void(*)()>(&exception_handler_noerr), 0x08, 0x8E);
        feron::cpu::idt::set_idt_entry(4,  reinterpret_cast<void(*)()>(&exception_handler_noerr), 0x08, 0x8E);
        feron::cpu::idt::set_idt_entry(5,  reinterpret_cast<void(*)()>(&exception_handler_noerr), 0x08, 0x8E);
        feron::cpu::idt::set_idt_entry(6,  reinterpret_cast<void(*)()>(&isr_invalid_opcode),     0x08, 0x8E);
        feron::cpu::idt::set_idt_entry(7,  reinterpret_cast<void(*)()>(&exception_handler_noerr), 0x08, 0x8E);
        feron::cpu::idt::set_idt_entry(8,  reinterpret_cast<void(*)()>(&exception_handler),       0x08, 0x8E);
        feron::cpu::idt::set_idt_entry(9,  reinterpret_cast<void(*)()>(&exception_handler_noerr), 0x08, 0x8E);
        feron::cpu::idt::set_idt_entry(10, reinterpret_cast<void(*)()>(&exception_handler),       0x08, 0x8E);
        feron::cpu::idt::set_idt_entry(11, reinterpret_cast<void(*)()>(&exception_handler),       0x08, 0x8E);
        feron::cpu::idt::set_idt_entry(12, reinterpret_cast<void(*)()>(&exception_handler),       0x08, 0x8E);
        feron::cpu::idt::set_idt_entry(13, reinterpret_cast<void(*)()>(&exception_handler),       0x08, 0x8E);
        feron::cpu::idt::set_idt_entry(14, reinterpret_cast<void(*)()>(&isr_page_fault),          0x08, 0x8E);
        feron::cpu::idt::set_idt_entry(15, reinterpret_cast<void(*)()>(&exception_handler_noerr), 0x08, 0x8E);
        feron::cpu::idt::set_idt_entry(16, reinterpret_cast<void(*)()>(&exception_handler_noerr), 0x08, 0x8E);
        feron::cpu::idt::set_idt_entry(17, reinterpret_cast<void(*)()>(&exception_handler),       0x08, 0x8E);
        feron::cpu::idt::set_idt_entry(18, reinterpret_cast<void(*)()>(&exception_handler_noerr), 0x08, 0x8E);
        feron::cpu::idt::set_idt_entry(19, reinterpret_cast<void(*)()>(&exception_handler_noerr), 0x08, 0x8E);
        feron::cpu::idt::set_idt_entry(20, reinterpret_cast<void(*)()>(&exception_handler_noerr), 0x08, 0x8E);
        for (int i = 21; i <= 31; ++i)
            feron::cpu::idt::set_idt_entry(i, reinterpret_cast<void(*)()>(&exception_handler_noerr), 0x08, 0x8E);
    }
}
