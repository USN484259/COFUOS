[bits 64]

get_thread equ 0x100
close_handle equ 0x0308
exit_process equ 0x0210

section .text

mov eax,get_thread
syscall

mov ebx,eax
mov edx,eax
mov eax,close_handle
syscall

mov edx,ebx
mov eax,close_handle
syscall

mov edx,eax
mov eax,exit_process
syscall