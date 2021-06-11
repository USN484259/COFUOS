[bits 64]

exit_process equ 0x0210

section .text

mov rbx,0xFFFF800000001000

mov eax,exit_process
mov edx,[rbx]
syscall

