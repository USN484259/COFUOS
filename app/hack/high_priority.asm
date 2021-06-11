[bits 64]

get_thread equ 0x0100
get_priority equ 0x010C
set_priority equ 0x011C
exit_process equ 0x0210

section .text

mov eax,get_thread
syscall
mov ebx,eax

step:
mov edx,ebx
mov eax,get_priority
syscall

dec eax
js exit

mov edx,ebx
mov r8d,eax
mov eax,set_priority
syscall

test eax,eax
jz step

exit:
mov edx,eax
mov eax,exit_process
syscall