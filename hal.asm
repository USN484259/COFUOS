[bits 64]



global memset
global zeromemory
global IF_get
global IF_set

global _rdmsr
global _wrmsr

global buildIDT

section .text


;void* memset(void*,int,size_t)
memset:

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
ret



_memset:

mov rax,rdx
mov rcx,r8
_badalign:


rep stosb

pop rdi
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


buildIDT:




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