[bits 64]

global memset
global memcpy
global zeromemory

section .text

memset_val:
test r8,r8
mov [rsp+8],rdi
mov [rsp+0x10],rcx
jz .zero
mov al,dl
mov rdi,rcx
mov rcx,r8
rep stosb
mov rdi,[rsp+8]
.zero:
mov rax,[rsp+0x10]
ret

align 16
memset:
test dl,dl
jnz memset_val
mov rdx,r8
;NOP DWORD ptr [EAX + EAX*1 + 00000000H]
db 0Fh,1Fh,84h,00h,00h,00h,00h,00h

align 16
zeromemory:		;rcx dst	rdx size
test rdx,rdx
mov r9,rcx
mov r8d,ecx
jz .zero
mov [rsp+8],rdi
mov [rsp+0x10],rdx
mov ecx,8
xor rax,rax
and r8d,0x07
mov rdi,r9
jz .head_aligned
sub cl,r8b
sub rdx,rcx
js .small
rep stosb

.head_aligned:
;rdx remaining size
cmp rdx,8
mov rcx,rdx
js .tail_misalign
shr rcx,3
and dl,0x07
rep stosq
jnz .tail_misalign

mov rdi,[rsp+8]
mov rax,r9
ret

.small:
mov rdx,[rsp+0x10]
.tail_misalign:
movzx rcx,dl
rep stosb

mov rdi,[rsp+8]
.zero:
mov rax,r9
ret

align 16
memcpy:		;rcx dst	rdx sor		r8 size
test r8,r8
mov [rsp+8],rsi
mov [rsp+0x10],rdi
jz .zero
mov [rsp+0x18],rcx	;dst
mov [rsp+0x20],r8	;size
mov rsi,rdx
mov rax, .align_table
xor rdx,rcx
mov rdi,rcx
and rdx,0x07
mov r9d,ecx
jmp [rax + 8*rdx]

.zero:
mov rax,rcx
ret

align 16
.align_table:
dq .align_8
dq .align_def
dq .align_2
dq .align_def
dq .align_4
dq .align_def
dq .align_2
dq .align_def

align 8
.align_8:
mov ecx,8
and r9b,0x07
jz .align_8_skip
sub cl,r9b
sub r8,rcx
jbe NEAR .small
rep movsb

.align_8_skip:
cmp r8,8
mov rcx,r8
js NEAR .align_def
shr rcx,3
and r8,0x07
rep movsq
jnz NEAR .align_def

mov rsi,[rsp+8]
mov rdi,[rsp+0x10]
mov rax,[rsp+0x18]
ret

align 8
.align_4:
mov ecx,4
and r9b,0x03
jz .align_4_skip
sub cl,r9b
sub r8,rcx
jbe .small
rep movsb

.align_4_skip:
cmp r8,4
mov rcx,r8
js .align_def
shr rcx,2
and r8,0x03
rep movsd
jnz .align_def

mov rsi,[rsp+8]
mov rdi,[rsp+0x10]
mov rax,[rsp+0x18]
ret

align 8
.align_2:
bt ecx,0
jnc .align_2_skip
dec r8
jz .small
movsb

.align_2_skip:
cmp r8,2
mov rcx,r8
js .align_2_once
shr rcx,1
rep movsw
jc .align_2_once

mov rsi,[rsp+8]
mov rdi,[rsp+0x10]
mov rax,[rsp+0x18]
ret

.align_2_once:
movsb
mov rsi,[rsp+8]
mov rdi,[rsp+0x10]
mov rax,[rsp+0x18]
ret

align 8
.small:
mov r8,[rsp+0x20]

align 8
.align_def:
mov rcx,r8
rep movsb
mov rsi,[rsp+8]
mov rdi,[rsp+0x10]
mov rax,[rsp+0x18]
ret