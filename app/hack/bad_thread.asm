[bits 64]

create_thread equ 0x0120
exit_thread equ 0x0110

section .data
fun:
mov eax,exit_thread
syscall

section .text

mov rdx,fun
xor r9,r9
mov eax,create_thread
syscall

mov eax,exit_thread
syscall