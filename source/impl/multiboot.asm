; source/impl/multiboot.asm
BITS 32
GLOBAL _start
GLOBAL pml4
GLOBAL pdpt
GLOBAL pd0
extern kernel_main
extern kernel_main

SECTION .multiboot
align 8
mb2_header_start:
    dd 0xE85250D6
    dd 0x0
    dd mb2_header_end - mb2_header_start
    dd -(0xE85250D6 + 0 + (mb2_header_end - mb2_header_start))
    dd 0
    dd 8
mb2_header_end:

SECTION .bss
align 4096
pml4:    resb 4096
pdpt:    resb 4096
pd0:     resb 4096

SECTION .data
align 8
gdt64:
    dq 0x0000000000000000                ; null
    dq 0x00AF9A000000FFFF                ; code
    dq 0x00AF92000000FFFF                ; data
gdt64_ptr:
    dw (3*8)-1
    dd gdt64

SECTION .bss
align 8
saved_magic: resd 1
saved_mbi:   resd 1

SECTION .text
BITS 32
_start:
    ; save Multiboot2 args (eax=magic, ebx=mbi)
    mov [saved_magic], eax
    mov [saved_mbi],   ebx

    ; enable PAE
    mov eax, cr4
    or  eax, 1 << 5
    mov cr4, eax

    ; clear tables
    lea edi, [pml4]
    mov ecx, 4096/4
    xor eax, eax
    rep stosd

    lea edi, [pdpt]
    mov ecx, 4096/4
    xor eax, eax
    rep stosd

    lea edi, [pd0]
    mov ecx, 4096/4
    xor eax, eax
    rep stosd

    ; PD0: 2 MiB huge page for 0..2MiB
    mov dword [pd0], 0x00000083
    mov dword [pd0+4], 0x00000000

    ; PDPT -> PD0
    mov eax, pd0
    or  eax, 0x003
    mov [pdpt], eax
    mov dword [pdpt+4], 0

    ; PML4 -> PDPT
    mov eax, pdpt
    or  eax, 0x003
    mov [pml4], eax
    mov dword [pml4+4], 0

    ; load CR3
    mov eax, pml4
    mov cr3, eax

    ; enable LME
    mov ecx, 0xC0000080
    rdmsr
    or eax, 1 << 8
    wrmsr

    ; enable paging
    mov eax, cr0
    or  eax, 1 << 31
    mov cr0, eax

    ; load 64-bit segments
    lgdt [gdt64_ptr]

    ; far jump to 64-bit code segment (selector 0x08)
    jmp 0x08:long_mode_entry

BITS 64
long_mode_entry:
    ; set up a stack within identity-mapped region (adjust once you map more)
    mov rsp, 0x0000000000200000

    ; load saved args (zero-extend to 64-bit)
    mov eax, dword [saved_magic]
    mov ebx, dword [saved_mbi]
    mov rdi, rax                  ; arg1: magic
    mov rsi, rbx                  ; arg2: mbi pointer (identity mapped)

    extern kernel_main
    sti
    call kernel_main

.hang:
    jmp .hang
