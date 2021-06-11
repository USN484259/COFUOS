[bits 64]

exit_process equ 0x0210
stream_write equ 0x050C

section .data
str:
db 'Hello World',0x0A,0

section .text

mov edx,2
mov r8,str
xor r9,r9
mov eax,stream_write
syscall

mov edx,eax
mov eax,exit_process
syscall