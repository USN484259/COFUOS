[bits 64]

exit_process equ 0x0210
vm_peek equ 0x0400
vm_protect equ 0x0408

section .text

main:
; rdx as image base
mov rcx,(~0x0FFF)
mov eax,vm_protect
mov r8d,1
and rdx,rcx
mov r9,0x00000000_0000001F
mov rbx,rdx
syscall

test eax,eax
jnz exit

mov rdx,rbx
mov eax,vm_peek
syscall

exit:
mov edx,eax
mov eax,exit_process
syscall