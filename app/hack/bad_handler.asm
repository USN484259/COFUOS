[bits 64]

set_handler equ 0x0118
exit_process equ 0x0210

section .text

mov rdx,0x0000800000001000
mov eax,set_handler
syscall

mov edx,eax
mov eax,exit_process
syscall