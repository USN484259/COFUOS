[bits 64]

create_thread equ 0x0120
sleep equ 0x0130
exit_thread equ 0x0110
exit_process equ 0x0210
vm_release equ 0x041C

section .text

main:
mov r8,rsp
mov rdx,stealer
mov eax,create_thread
xor r9,r9
syscall

call yield

xor rdx,rdx
mov eax,exit_process
syscall

yield:
mov rdx,1000*1000
mov eax,sleep
syscall
ret

stealer:
; rdx as arg
mov rcx,(~0x0FFF)
mov eax,vm_release
mov r8d,1
and rdx,rcx
syscall

mov eax,exit_thread
syscall