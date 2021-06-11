[bits 64]

get_thread equ 0x0100
stream_state equ 0x0500
exit_process equ 0x0210

section .text

mov eax,get_thread
syscall

mov edx,eax
mov eax,stream_state
syscall

mov edx,eax
mov eax,exit_process
syscall