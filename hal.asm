[bits 64]

HIGHADDR equ 0xFFFF8000_00000000

extern dispatch_exception


global buildIDT


global memset
global zeromemory
global IF_get
global IF_set
global CR3_get
global CR3_set
global _invlpg
global io_inb
global io_inw
global io_ind
global io_outb
global io_outw
global io_outd

global _rdmsr
global _wrmsr

global _invlpg


global _cmpxchgb
global _cmpxchgw
global _cmpxchgd

global dbgbreak

global BugCheck

ISR_STK_ALTER equ HIGHADDR+0x1800

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
mov rdx,[rbp+8*16]	;errcode
sub rcx,rax
mov rsp,ISR_STK_ALTER
shr rcx,3	;8 byte alignment
mov r8,rbp	;context
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


buildIDT:
push rsi
push rdi
mov rcx,20
mov rdi,HIGHADDR+IDT_BASE
mov rsi,ISR_exception
push rdi
mov rdx,HIGHADDR>>32
.exception:

mov eax,esi
mov ax,1000_1110_0000_0000_b
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

;void* memset(void*,int,size_t)
memset:

mov r9,rcx
push rdi
test rdx,rdx
mov rdi,rcx
jnz _memset

mov rdx,r8

;zeromemory dst,size
zeromemory:

mov rcx,rdx
xor rax,rax
shr rcx,3

rep stosq

and dl,7
movzx rcx,dl

jnz _badalign

.ret:
pop rdi
mov rax,r9
ret



_memset:

mov rax,rdx
mov rcx,r8
_badalign:


rep stosb

pop rdi
mov rax,r9
ret






;IF_get return IF
IF_get:

pushf
xor rax,rax
pop rdx
bt rdx,9	;IF
setc al
ret



;IF_set rcx state
;return oldstate
IF_set:

xor rax,rax
pushf
test rcx,rcx
pop rdx
jz .z
;cmovnz rax,0x0200	;IF
mov eax,0x0200
.z:
btr rdx,9	;IF
setc cl

or rdx,rax

push rdx
movzx rax,cl
popf

ret


CR3_get:
mov rax,cr3
ret

CR3_set:
mov rax,cr3
mov cr3,rcx
ret


io_inb:

mov rdx,rcx
in al,dx
ret

io_inw:

mov rdx,rcx
in ax,dx
ret

io_ind:

mov rdx,rcx
in eax,dx
ret

io_outb:
mov rax,rdx
mov rdx,rcx
out dx,al
ret

io_outw:
mov rax,rdx
mov rdx,rcx
out dx,ax
ret

io_outd:
mov rax,rdx
mov rdx,rcx
out dx,eax
ret


_rdmsr:

rdmsr

shl rdx,32
or rax,rdx
ret

_wrmsr:

mov rax,rdx
shr rdx,32
wrmsr
ret


_invlpg:

invlpg [rcx]
add rcx,0x1000
cmp rdx,rcx
jae _invlpg
ret


_cmpxchgb:
mov rax,rdx
lock cmpxchg [rcx],r8b
ret

_cmpxchgw:
mov rax,rdx
lock cmpxchg [rcx],r8w
ret

_cmpxchgd:
mov rax,rdx
lock cmpxchg [rcx],r8d
ret


dbgbreak:
int3
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

mov rdx,rcx
mov ecx,0xFFFFFFFF
mov r8,rsp
call dispatch_exception


.reboot:

mov dx,0x92
in al,dx
or al,1
out dx,al
hlt
jmp .reboot
