[bits 64]

get_process equ 0x200
create_process equ 0x0220
exit_process equ 0x0210
get_command equ 0x020C

buffer_size equ 0x100

SHELL equ 0x20

struc STARTUP_INFO
.commandline	resq 1
.work_dir		resq 1
.environment	resq 1
.cmd_length		resd 1
.wd_length		resd 1
.env_length		resd 1
.privilege		resd 1
.std_handle		resd 3
				resd 1
endstruc

section .text

sub rsp,(buffer_size + STARTUP_INFO_size)

mov eax,get_process
syscall

mov edx,eax
lea r8,[rsp + STARTUP_INFO_size]
mov r9d,buffer_size
mov eax,get_command
syscall

mov rbx,rax
test eax,eax
jnz abort
shr rbx,32

mov rdi,rsp
xor rax,rax
mov rcx,(STARTUP_INFO_size/8)
cld
rep stosq

lea rdx,[rsp + STARTUP_INFO_size]
mov [rsp + STARTUP_INFO.cmd_length],ebx
mov [rsp + STARTUP_INFO.commandline],rdx
mov dword [rsp + STARTUP_INFO.privilege],SHELL

mov rdx,rsp
mov r8d,STARTUP_INFO_size
mov eax,create_process
syscall

mov edx,eax
mov eax,exit_process
syscall

abort:
ud2