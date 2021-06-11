[bits 64]

get_thread equ 0x0100
exit_process equ 0x0210

section .text
xor ebx,ebx
main:
mov eax,get_thread
syscall

test eax,eax
jz exit

inc ebx
jmp main

exit:

mov edx,ebx
mov eax,exit_process
syscall