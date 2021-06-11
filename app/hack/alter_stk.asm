[bits 64]

exit_process equ 0x0210
vm_reserve equ 0x0410
vm_commit equ 0x0414
vm_release equ 0x041C

section .text

mov eax,vm_reserve
xor rdx,rdx
mov r8d,1
syscall

mov rdx,rax
mov rbx,rax
mov r8d,1
mov eax,vm_commit
syscall

mov rcx,(~0x0FFF)
mov rdx,rsp
mov r8d,1
mov eax,vm_release
and rdx,rcx
mov rsp,rbx
syscall

xor rdx,rdx
mov eax,exit_process
syscall