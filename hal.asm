[bits 64]

global IF_get
global IF_set

section .text


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