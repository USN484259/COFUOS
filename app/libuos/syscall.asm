[bits 64]

global syscall

section .text

syscall:
mov rax,rcx
syscall
ret
