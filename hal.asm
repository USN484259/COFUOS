[bits 64]

HIGHADDR equ 0xFFFF8000_00000000


extern dispatch_exception


global buildIDT
global DR_match
global DR_get
global DR_set

global memset
global zeromemory
global memcpy

global serial_peek
global serial_get
global serial_put

global BugCheck
;global __C_specific_handler
global __chkstk

;global handler_push
;global handler_pop

section .text

ISR_exception:

.exp0:
push rax
call near exception_entry
int3
align 8
.exp1:
push rax
call near exception_entry
int3
align 8
.exp2:
push rax
call near exception_entry
int3
align 8
.exp3:
push rax
call near exception_entry
int3
align 8
.exp4:
push rax
call near exception_entry
int3
align 8
.exp5:
push rax
call near exception_entry
int3
align 8
.exp6:
push rax
call near exception_entry
int3
align 8
.exp7:
push rax
call near exception_entry
int3
align 8
.exp8:
call near exception_entry
int3
align 8
.exp9:
push rax
call near exception_entry
int3
align 8
.exp10:
call near exception_entry
int3
align 8
.exp11:
call near exception_entry
int3
align 8
.exp12:
call near exception_entry
int3
align 8
.exp13:
call near exception_entry
int3
align 8
.exp14:
call near exception_entry
int3
align 8
.exp15:
push rax
call near exception_entry
int3
align 8
.exp16:
push rax
call near exception_entry
int3
align 8
.exp17:
call near exception_entry
int3
align 8
.exp18:
push rax
call near exception_entry
int3
align 8
.exp19:
push rax
call near exception_entry
int3

align 16


exception_entry:

;SS
;rsp
;rflags
;CS
;rip
;errcode
;exp#
push rbp
push rax
push rcx
push rdx
push rbx
push rsi
push rdi

push r8
push r9
push r10
push r11
push r12
push r13
push r14
push r15

mov rbp,rsp
mov rcx,[rbp+8*15]	;exp#
mov rax,ISR_exception
;mov rdx,[rbp+8*16]	;errcode
mov rdx,rbp	;context
sub rcx,rax
;and sp,0xF800	;lower 2K
shr rcx,3	;8 byte alignment
;mov r8,rbp	;context
call dispatch_exception


mov rsp,rbp
pop r15
pop r14
pop r13
pop r12
pop r11
pop r10
pop r9
pop r8
pop rdi
pop rsi
pop rbx
pop rdx
pop rcx
pop rax
pop rbp

add rsp,0x10	;exp# and errcode

iretq

IDT_BASE equ 0x0400
IDT_LIM equ 0x400	;64 entries


;TODO	rcx as MP id
buildIDT:
movzx r8w,cl	;MP id
push rsi
push rdi
mov rcx,20
mov rdi,HIGHADDR+IDT_BASE
mov rsi,ISR_exception
or r8w,1000_1110_0000_0000_b
push rdi
mov rdx,HIGHADDR>>32
.exception:

mov eax,esi
mov ax,r8w	;1000_1110_0000_0000_b
shl rax,32
mov ax,si
bts rax,19	;8<<16
stosq
lodsq
mov rax,rdx
stosq

loop .exception

mov rcx,12*2
xor rax,rax
rep stosq	;gap


mov rax,(IDT_LIM-1) << 48
push rax
lidt [rsp+6]
pop rax
pop rax

pop rdi
pop rsi
ret

DR_match:
;pushf
;cli
mov rax,dr6
test al,1	;B0
jz .end
mov rdx,dr7
and dl,0xFC
mov dr7,rdx
.end:
;popf
ret


DR_get:

push rdi
mov rdi,rcx
mov rax,dr0
stosq
mov rax,dr1
stosq
mov rax,dr2
stosq
mov rax,dr3
stosq
mov rax,dr6
stosq
mov rax,dr7
stosq

pop rdi
ret


DR_set:
push rsi
mov rsi,rcx
lodsq
mov dr0,rax
lodsq
mov dr1,rax
lodsq
mov dr2,rax
lodsq
mov dr3,rax
lodsq
mov dr6,rax
lodsq
mov dr7,rax

pop rsi
ret

;void* memset(void*,int,size_t)
memset:

test rdx,rdx
jnz memset_nonzero
mov rdx,r8

zeromemory:
push rdi
mov rdi,rcx
mov rcx,rdx
xor rax,rax
shr rcx,3

rep stosq

and dl,7
jnz .badalign

pop rdi
ret

.badalign:
movzx rcx,dl
rep stosb
pop rdi
ret

memset_nonzero:
push rcx
push rdi
mov rdi,rcx
mov rax,rdx
mov rcx,r8
rep stosb

pop rdi
pop rax
ret


;void* memcpy(void*,const void*,size_t);
memcpy:

push rdi
push rsi

mov rdi,rcx
mov rsi,rdx
mov rcx,r8
mov rax,rdi
shr rcx,3
rep movsq

and r8b,7
jnz .misaligned

pop rsi
pop rdi
ret

.misaligned:
movzx rcx,r8b
rep movsb
pop rsi
pop rdi
ret


;byte serial_peek(word);
serial_peek:
mov dx,cx
add dx,5
in al,dx

and al,1
ret

;byte serial_get(word);
serial_get:
pause
call serial_peek
test al,al
jz serial_get

mov dx,cx
in al,dx
ret

;void serial_put(word,byte);
serial_put:
mov r8,rdx
mov dx,cx
add dx,5

.wait:
pause
in al,dx
test al,0x20
jz .wait

mov dx,cx
mov rax,r8
out dx,al
ret





;BugCheck status,arg1,arg2
BugCheck:

push rbp
push rax
push rcx
push rdx
push rbx
push rsi
push rdi

push r8
push r9
push r10
push r11
push r12
push r13
push r14
push r15

;mov rdx,rcx
mov ecx,0xFFFFFFFF
mov rdx,rsp
call dispatch_exception

.reboot:

mov dx,0x92
in al,dx
or al,1
out dx,al
hlt
jmp .reboot


__chkstk:
;TODO: probe every page
ret


;__C_specific_handler:


;xor rax,rax
;ret