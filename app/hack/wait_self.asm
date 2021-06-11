[bits 64]

get_thread equ 0x0100
wait_for equ 0x0134
exit_process equ 0x0210

section .text

mov eax,get_thread
syscall

xor ecx,ecx
mov edx,eax
mov r8,rcx
mov r9,rcx
mov eax,wait_for
syscall

mov edx,eax
mov eax,exit_process
syscall
