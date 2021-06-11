[bits 64]

set_handler equ 0x0118
exit_process equ 0x0210

section .data
handler:
xor rdx,rdx
mov eax,exit_process
syscall

section .text

main:
mov rdx,handler
mov eax,set_handler
syscall

ud2