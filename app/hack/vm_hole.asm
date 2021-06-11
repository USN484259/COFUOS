[bits 64]

exit_process equ 0x0210
vm_protect equ 0x0408

section .text

main:
; rdx as image base
sub rdx,0x1000
mov rcx,(~0x0FFF)
mov eax,vm_protect
mov r8d,1
mov r9,0x80000000_00000007
and rdx,rcx
syscall

mov edx,eax
mov eax,exit_process
syscall