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

    // Map leaf VA->PA (create tables as needed), writing table contents via temporary mappings
    uint64_t* walk_create(uint64_t va);

    inline bool map_page(uint64_t va, uint64_t pa, uint64_t flags = P_PRESENT | P_RW) {
        uint64_t* pte = walk_create(va);
        if (!pte) return false;
        *pte = (pa & ~0xFFFull) | (flags & ~P_PS);
        invlpg(va);
        return true;
    }

    // Temporary map/unmap helpers: get a VA page from valloc for scratch work
    inline uint64_t scratch_va = 0;

    inline bool map_scratch(uint64_t pa, uint64_t flags = P_PRESENT | P_RW) {
        if (scratch_va == 0) {
            scratch_va = feron::mm::valloc::alloc_range(feron::mm::pfa::PAGE_SIZE);
            if (scratch_va == 0) return false;
        }
        return map_page(scratch_va, pa, flags);
    }
    inline void* scratch_ptr() { return reinterpret_cast<void*>(scratch_va); }
    inline void unmap_scratch() {
        // Clear the leaf without freeing scratch_va, so we can reuse it.
        auto idx = [](uint64_t v, int s){ return (v >> s) & 0x1FF; };
        int i4 = idx(scratch_va, 39), i3 = idx(scratch_va, 30), i2 = idx(scratch_va, 21), i1 = idx(scratch_va, 12);
        uint64_t pml4e = PML4_va[i4]; if (!(pml4e & P_PRESENT)) return;
        auto pdpt_va = reinterpret_cast<uint64_t*>( (pml4e & ~0xFFFull) ); // mapped VA for PDPT (see below)
        // We will always touch tables via scratch; leaf clearing is done by mapping PT page into scratch, but since we know scratch_va’s PT entry,
        // it’s simpler to just overwrite the PTE via the same walk_create path. For brevity, re-map scratch to a zero PA or write zero through PT:
        // Easiest: re-walk to the leaf and write 0:
        auto leaf = walk_create(scratch_va);
        if (leaf) { *leaf = 0; invlpg(scratch_va); }
    }

    // Allocate and zero a page table using scratch map (no identity)
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

        // PML4_va must be a real VA we can dereference (we map it permanently)
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
        // Caller writes then we unmap scratch; return a transient pointer valid until unmap.
        return leaf;
    }

    // Initialize paging with caller-provided VA pool and optional initial maps
    inline void init(uint64_t va_pool_base, uint64_t va_pool_size,
                     uint64_t initial_map_va = 0, uint64_t initial_map_pa = 0, uint64_t initial_map_size = 0,
                     uint64_t leaf_flags = P_PRESENT | P_RW) {
        // Initialize VA allocator pool (caller decides where/how big)
        feron::mm::valloc::init(va_pool_base, va_pool_size);

        // Create PML4
        PML4_pa = alloc_table_pa();
        if (!PML4_pa) return;

        // Permanently map PML4 itself into the VA pool so we can dereference entries
        uint64_t pml4_va_page = feron::mm::valloc::alloc_range(feron::mm::pfa::PAGE_SIZE);
        if (!pml4_va_page) return;
        map_page(pml4_va_page, PML4_pa, P_PRESENT | P_RW);
        PML4_va = reinterpret_cast<uint64_t*>(pml4_va_page);

        // Load CR3 with physical root
        asm volatile("mov %0, %%cr3" : : "r"(PML4_pa) : "memory");

        // Optional: map an initial range (no hardcoded sizes—caller chooses)
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
