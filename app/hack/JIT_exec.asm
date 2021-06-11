[bits 64]

get_thread equ 0x0100
kill_thread equ 0x0114
create_thread equ 0x0120
wait_for equ 0x0134
exit_process equ 0x0210
vm_protect equ 0x0408
vm_reserve equ 0x0410
vm_commit equ 0x0414

section .data
fun:
; rdx as arg
mov eax,kill_thread
syscall

mov edx,eax
mov eax,exit_process
syscall
align 16

fun_size equ ($ - fun)

section .text

xor rdx,rdx
mov r8d,1
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

mov rsi,fun
mov rdi,rbx
mov ecx,(fun_size / 8)
cld
rep movsq

mov rdx,rbx
mov r8d,1
mov r9,0x00000000_00000005
mov eax,vm_protect
syscall

test eax,eax
jnz abort

mov eax,get_thread
syscall

mov r8d,eax
mov rdx,rbx
xor r9,r9
mov eax,create_thread
syscall

mov rdx,rax
test eax,eax
jnz abort

xor rcx,rcx
shr rdx,32
mov r8,rcx
mov r9,rcx
mov eax,wait_for
syscall

abort:
ud2