[bits 64]

get_thread equ 0x0100
wait_for equ 0x0134
create_thread equ 0x0120
exit_process equ 0x0210

section .text

main:
mov eax,get_thread
syscall

mov r8d,eax
mov rdx,waiter
xor r9,r9
mov eax,create_thread
syscall

shr rax,32
xor ecx,ecx
mov edx,eax
mov r8,rcx
mov r9,rcx
mov eax,wait_for
syscall

mov edx,eax
mov eax,exit_process
syscall


waiter:
xor rcx,rcx
; rdx as arg
mov r8,rcx
mov r9,rcx
mov eax,wait_for
syscall

mov edx,eax
mov eax,exit_process
syscall