[bits 64]

exit_process equ 0x0210

section .text
main:
push rax
jmp main