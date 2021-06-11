[bits 64]

close_handle equ 0x0308
exit_process equ 0x0210
stream_write equ 0x050C

section .data
str:
db 'Hello World',0x0A,0
str_len equ ($ - str)

section .text

mov ebx,3
close:
mov edx,ebx
mov eax,close_handle
syscall

dec ebx
jns close

mov edx,2
mov r8,str
mov r9d,str_len
mov eax,stream_write
syscall

mov edx,eax
mov eax,exit_process
syscall