[bits 64]

exit_process equ 0x0210
vm_release equ 0x041C

section .text

main:
; rdx as image base
mov rcx,(~0x0FFF)
mov eax,vm_release
and rdx,rcx
mov r8d,1
mov rbx,rdx
syscall

test eax,eax
jnz abort

mov rdx,rbx
mov eax,vm_release
mov r8d,1
syscall

mov edx,eax
mov eax,exit_process
syscall

abort:
ud2