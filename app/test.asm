[bits 64]

section .text
main:
mov rsi,r8	;command
mov ebx,4
.echo:
xor eax,eax
mov rdx,rsi
syscall
mov eax,0x0100
mov edx,(1000*1000)
syscall
dec ebx
jns .echo
mov eax,0x0108
syscall
int3