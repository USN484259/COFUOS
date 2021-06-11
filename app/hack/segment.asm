[bits 64]

exit_process equ 0x0210

section .text
main:
nop
nop
nop
nop
mov ax,0x10
mov ds,ax

mov rcx,main
mov eax,exit_process
mov edx,[rcx]
syscall