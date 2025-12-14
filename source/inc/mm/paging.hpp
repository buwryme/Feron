#pragma once

#include <cstdint>
#include "pfa.hpp"
#include "valloc.hpp"

namespace feron::mm::paging {
    constexpr uint64_t P_PRESENT  = 1ull << 0;
    constexpr uint64_t P_RW       = 1ull << 1;
    constexpr uint64_t P_USER     = 1ull << 2;
    constexpr uint64_t P_PS       = 1ull << 7;
    constexpr uint64_t P_NX       = 1ull << 63;

    inline uint64_t* PML4_va = nullptr;  // VA of root (mapped)
    inline uint64_t  PML4_pa = 0;        // physical address loaded into CR3

    inline void invlpg(uint64_t va) { asm volatile("invlpg (%0)" : : "r"(va) : "memory"); }

    // Scratch helpers MUST be declared before any use
    inline uint64_t scratch_va = 0;

    inline void* scratch_ptr() {
        return reinterpret_cast<void*>(scratch_va);
    }

    inline void unmap_scratch() {
        if (!PML4_va || scratch_va == 0) return;

        auto idx = [](uint64_t v, int s){ return (v >> s) & 0x1FF; };
        int i4 = idx(scratch_va, 39), i3 = idx(scratch_va, 30), i2 = idx(scratch_va, 21), i1 = idx(scratch_va, 12);

        // Walk to the PT that contains scratch_va and clear the leaf
        uint64_t pml4e = PML4_va[i4];
        if (!(pml4e & P_PRESENT)) return;
        uint64_t pdpt_pa = (pml4e & ~0xFFFull);

        // Map PDPT page into scratch to read its entry
        if (!scratch_va) return;
        // We re-use map_scratch once available; if it fails, just return quietly
        extern bool map_scratch(uint64_t pa, uint64_t flags);
        if (!map_scratch(pdpt_pa, P_PRESENT | P_RW)) return;
        auto PDPT = reinterpret_cast<uint64_t*>(scratch_ptr());
        uint64_t pdpte = PDPT[i3];
        uint64_t pd_pa = (pdpte & P_PRESENT) ? (pdpte & ~0xFFFull) : 0;
        // Clear scratch mapping of PDPT
        // We can simply proceed; scratch leaf will be overwritten below.

        if (!pd_pa) return;
        if (!map_scratch(pd_pa, P_PRESENT | P_RW)) return;
        auto PD = reinterpret_cast<uint64_t*>(scratch_ptr());
        uint64_t pde = PD[i2];
        uint64_t pt_pa = (pde & P_PRESENT) ? (pde & ~0xFFFull) : 0;

        if (!pt_pa) return;
        if (!map_scratch(pt_pa, P_PRESENT | P_RW)) return;
        auto PT = reinterpret_cast<uint64_t*>(scratch_ptr());
        PT[i1] = 0;
        invlpg(scratch_va);
    }

    // Forward declare walk_create before map_page uses it
    uint64_t* walk_create(uint64_t va);

    inline bool map_page(uint64_t va, uint64_t pa, uint64_t flags = P_PRESENT | P_RW) {
        uint64_t* pte = walk_create(va);
        if (!pte) return false;
        *pte = (pa & ~0xFFFull) | (flags & ~P_PS);
        invlpg(va);
        return true;
    }

    // Map a physical page at scratch_va so we can write its contents
    inline bool map_scratch(uint64_t pa, uint64_t flags = P_PRESENT | P_RW) {
        if (scratch_va == 0) {
            scratch_va = feron::mm::valloc::alloc_range(feron::mm::pfa::PAGE_SIZE);
            if (scratch_va == 0) return false;
        }
        if (!PML4_va) {
            // Pre-CR3 path: we cannot create a mapping via walk_create yet.
            // Callers should only use scratch pre-CR3 for physical writes after identity is set.
            return false;
        }
        return map_page(scratch_va, pa, flags);
    }

    // Allocate and zero a page table using scratch map (post-CR3)
    inline uint64_t alloc_table_pa() {
        uint64_t pa = feron::mm::pfa::alloc_page();
        if (!pa) return 0;
        if (!map_scratch(pa)) return 0;
        memset(scratch_ptr(), 0, feron::mm::pfa::PAGE_SIZE);
        unmap_scratch();
        return pa;
    }

    // Walk tables; table contents are modified through scratch mappings
    inline uint64_t* walk_create(uint64_t va) {
        auto idx = [](uint64_t v, int s){ return (v >> s) & 0x1FF; };
        int i4 = idx(va, 39), i3 = idx(va, 30), i2 = idx(va, 21), i1 = idx(va, 12);

        if (!PML4_va) return nullptr;

        // PDPT
        uint64_t pml4e = PML4_va[i4];
        uint64_t pdpt_pa = (pml4e & P_PRESENT) ? (pml4e & ~0xFFFull) : 0;
        if (!pdpt_pa) {
            pdpt_pa = alloc_table_pa();
            if (!pdpt_pa) return nullptr;
            PML4_va[i4] = (pdpt_pa & ~0xFFFull) | P_PRESENT | P_RW;
        }

        // Touch PDPT via scratch
        if (!map_scratch(pdpt_pa)) return nullptr;
        auto PDPT = reinterpret_cast<uint64_t*>(scratch_ptr());
        uint64_t pdpte = PDPT[i3];
        uint64_t pd_pa = (pdpte & P_PRESENT) ? (pdpte & ~0xFFFull) : 0;
        if (!pd_pa) {
            pd_pa = alloc_table_pa();
            if (!pd_pa) { unmap_scratch(); return nullptr; }
            PDPT[i3] = (pd_pa & ~0xFFFull) | P_PRESENT | P_RW;
        }
        unmap_scratch();

        // PD
        if (!map_scratch(pd_pa)) return nullptr;
        auto PD = reinterpret_cast<uint64_t*>(scratch_ptr());
        uint64_t pde = PD[i2];
        uint64_t pt_pa = (pde & P_PRESENT) ? (pde & ~0xFFFull) : 0;
        if (!pt_pa) {
            pt_pa = alloc_table_pa();
            if (!pt_pa) { unmap_scratch(); return nullptr; }
            PD[i2] = (pt_pa & ~0xFFFull) | P_PRESENT | P_RW;
        }
        unmap_scratch();

        // PT
        if (!map_scratch(pt_pa)) return nullptr;
        auto PT = reinterpret_cast<uint64_t*>(scratch_ptr());
        uint64_t* leaf = &PT[i1];
        return leaf;
    }

    // Initialize paging with safe 4 MiB identity before CR3 switch
    inline void init(uint64_t va_pool_base, uint64_t va_pool_size,
                     uint64_t initial_map_va = 0, uint64_t initial_map_pa = 0, uint64_t initial_map_size = 0,
                     uint64_t leaf_flags = P_PRESENT | P_RW) {
        feron::mm::valloc::init(va_pool_base, va_pool_size);

        // Create tables physically and zero via current identity (from trampoline)
        auto memzero_phys = [](uint64_t pa){
            volatile uint8_t* p = reinterpret_cast<volatile uint8_t*>(pa);
            for (std::size_t i = 0; i < feron::mm::pfa::PAGE_SIZE; ++i) p[i] = 0;
        };

        PML4_pa = feron::mm::pfa::alloc_page();
        uint64_t pdpt_pa = feron::mm::pfa::alloc_page();
        uint64_t pd_pa   = feron::mm::pfa::alloc_page();
        uint64_t pt_pa   = feron::mm::pfa::alloc_page();
        if (!PML4_pa || !pdpt_pa || !pd_pa || !pt_pa) return;

        memzero_phys(PML4_pa);
        memzero_phys(pdpt_pa);
        memzero_phys(pd_pa);
        memzero_phys(pt_pa);

        // Wire PML4[0] -> PDPT, PDPT[0] -> PD, PD[0] -> PT
        volatile uint64_t* PML4phys = reinterpret_cast<volatile uint64_t*>(PML4_pa);
        volatile uint64_t* PDPTphys = reinterpret_cast<volatile uint64_t*>(pdpt_pa);
        volatile uint64_t* PDphys   = reinterpret_cast<volatile uint64_t*>(pd_pa);
        volatile uint64_t* PTphys   = reinterpret_cast<volatile uint64_t*>(pt_pa);

        PML4phys[0] = (pdpt_pa & ~0xFFFull) | P_PRESENT | P_RW;
        PDPTphys[0] = (pd_pa   & ~0xFFFull) | P_PRESENT | P_RW;
        PDphys[0]   = (pt_pa   & ~0xFFFull) | P_PRESENT | P_RW;

        // Identity map 0..4 MiB (4 KiB pages)
        for (uint64_t off = 0, i = 0; i < 1024; ++i, off += feron::mm::pfa::PAGE_SIZE) {
            PTphys[i] = (off & ~0xFFFull) | leaf_flags;
        }

        // Load CR3 to the new PML4
        asm volatile("mov %0, %%cr3" : : "r"(PML4_pa) : "memory");

        // Permanently map PML4 into VA space and set PML4_va
        uint64_t pml4_va_page = feron::mm::valloc::alloc_range(feron::mm::pfa::PAGE_SIZE);
        if (pml4_va_page == 0) return;

        // Use identity PT to map PML4_pa at pml4_va_page
        uint64_t slot = (pml4_va_page >> 12) & 0x3FF;
        PTphys[slot] = (PML4_pa & ~0xFFFull) | P_PRESENT | P_RW;
        PML4_va = reinterpret_cast<uint64_t*>(pml4_va_page);

        // Optional initial map
        if (initial_map_size) {
            uint64_t va = initial_map_va;
            uint64_t pa = initial_map_pa;
            uint64_t end = pa + initial_map_size;
            for (; pa < end; pa += feron::mm::pfa::PAGE_SIZE, va += feron::mm::pfa::PAGE_SIZE) {
                map_page(va, pa, leaf_flags);
            }
        }
    }

    // Map a range dynamically (size multiple of page size)
    inline bool map_range(uint64_t va, uint64_t pa, uint64_t size, uint64_t flags) {
        uint64_t end = pa + size;
        for (; pa < end; pa += feron::mm::pfa::PAGE_SIZE, va += feron::mm::pfa::PAGE_SIZE) {
            if (!map_page(va, pa, flags)) return false;
        }
        return true;
    }
}
