[bits 64]

exit_process equ 0x0210

section .text

mov eax,0x1000
syscall

mov eax,exit_process
xor rdx,rdx
syscall