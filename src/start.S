.syntax unified
.arm
.section .text.start,"ax"
.global _start
_start:
    b init_module
    .word mod_header - _start

mod_header:
    .ascii "MOD0"
    .word dynamic_table - mod_header
    .word bss_start - mod_header
    .word bss_end - mod_header
    .word 0, 0
    .word module_object - mod_header

// DT_INIT is called by the guest dynamic linker. The guard also permits
// loaders that invoke the module's entry before its initializer.
init_module:
    ldr r2, guard_offset
guard_pc:
    add r2, pc, r2
    ldr r3, [r2]
    cmp r3, #0
    bxne lr
    mov r3, #1
    str r3, [r2]
    adr r0, message
    ldr r1, message_length
    svc #0x27
    bx lr
message_length:
    .word message_end - message
guard_offset:
    .word initialized - (guard_pc + 8)
message:
    .ascii "MHGU-Damage-Display initialized\n"
message_end:

.section .rodata,"a"
    .word 0, name_end - name
name:
    .ascii "MHGU-Damage-Display"
name_end:
    .byte 0
    .balign 4
hash_table:
    .word 1, 1, 0, 0
symbols:
    .space 16
strings:
    .byte 0
.section .data,"aw"
.global dynamic_table
dynamic_table:
    .word 4, hash_table
    .word 5, strings
    .word 6, symbols
    .word 10, 1
    .word 11, 16
    .word 12, init_module
    .word 0, 0
.section .bss,"aw",%nobits
    .balign 16
module_object:
    .space 0x200
initialized:
    .space 4
