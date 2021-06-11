[bits 64]

exit_process equ 0x0210
stream_write equ 0x050C

section .text

mov edx,2
xor r8,r8
mov r9d,0x10
mov eax,stream_write
syscall

mov edx,eax
mov eax,exit_process
syscall