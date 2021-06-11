[bits 64]

exit_process equ 0x0210
vm_reserve equ 0x0410
vm_commit equ 0x0414
stream_read equ 0x0508

section .text

xor rdx,rdx
mov r8d,2
mov eax,vm_reserve
syscall

test rax,rax
mov rdx,rax
jz abort

mov r8d,1
mov eax,vm_commit
mov rbx,rdx
syscall

test eax,eax
jnz abort

mov edx,1
lea r8,[rbx + 0x0FF0]
mov r9d,0x20
mov eax,stream_read
syscall

mov edx,eax
mov eax,exit_process
syscall

abort:
ud2