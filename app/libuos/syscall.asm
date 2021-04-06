[bits 64]

global syscall

section .text

syscall:
mov rax,rcx
syscall
mov cx,ss
mov ds,cx
mov es,cx
ret
