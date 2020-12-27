[bits 64]

HIGHADDR equ 0xFFFF8000_00000000

IDT_BASE equ 0x0800
IDT_LIM equ 0x400	;64 entries

;WARNING: according to x64 calling convention 0x20 space on stack needed before calling C functions

extern dispatch_exception
extern dispatch_irq

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

; 0 ~ 7, 9, 15, 16, 18, 19 no errcode
; 8, 10 ~ 14, 17 has errcode

%macro ISR_STUB 1
%if %1 == 8 || %1 == 17 || (%1 >= 10 && %1 <= 14)
;
%else
push rax
%endif
call near exception_entry
int3
align 8
%endmacro

align 16
ISR_exception:
%assign i 0
%rep 20
ISR_STUB i
%assign i i+1
%endrep

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

sub rsp,0x20

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

align 16
ISR_irq:
%rep (IDT_LIM/0x10 - 0x20)
call near irq_entry
int3
align 8
%endrep

irq_entry:
xchg rcx,[rsp]
push rax
push rdx
push r8
push r9
mov rax,ISR_irq
push r10
push r11
sub rcx,rax
sub rsp,0x20
shr rcx,3
call dispatch_irq
add rsp,0x20

pop r11
pop r10
pop r9
pop r8
pop rdx
pop rax
pop rcx
iretq




align 16
buildIDT:
mov [rsp+0x10],rdi
mov [rsp+0x08],rsi
mov ecx,20
mov rdi,HIGHADDR+IDT_BASE
mov rsi,ISR_exception
mov rdx,HIGHADDR>>32

.exception:
mov eax,esi
mov ax,1000_1110_0000_0000_b
shl rax,32
mov ax,si
bts rax,19	;8<<16
stosq
mov rax,rdx
add rsi,8
stosq
loop .exception

mov ecx,12*2
xor rax,rax
rep stosq   ;gap

mov rsi,ISR_irq
;rdx == HIGHADDR>>32
mov ecx,IDT_LIM/0x10 - 0x20

.isr:
mov eax,esi
mov ax,1000_1110_0000_0000_b
shl rax,32
mov ax,si
bts rax,19	;8<<16
stosq
mov rax,rdx
add rsi,8
stosq
loop .isr

mov rdi,[rsp+0x10]
mov rsi,[rsp+0x08]
ret

align 16
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

align 16
DR_get:

mov [rsp+8],rdi
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

mov rdi,[rsp+8]
ret

align 16
DR_set:
mov [rsp+8],rsi
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

mov rsi,[rsp+8]
ret

align 16
memset_val:
mov [rsp+8],rdi
mov [rsp+0x10],rcx
mov al,dl
mov rdi,rcx
mov rcx,r8
rep stosb
mov rdi,[rsp+8]
mov rax,[rsp+0x10]
ret

align 16
memset:
test edx,edx
jnz memset_val
mov rdx,r8

align 16
zeromemory:		;rcx dst	rdx size
mov r9,rcx
mov r8d,ecx
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
mov rax,r9
ret

align 16
memcpy:		;rcx dst	rdx sor		r8 size
mov [rsp+8],rsi
mov [rsp+0x10],rdi
mov [rsp+0x18],rcx	;dst
mov [rsp+0x20],r8	;size
mov rsi,rdx
mov rax, .align_table
xor rdx,rcx
mov rdi,rcx
and rdx,0x07
mov r9d,ecx
jmp [rax + 8*rdx]

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


;byte serial_peek(word);
align 16
serial_peek:
mov dx,cx
add dx,5
in al,dx

and al,1
ret

;byte serial_get(word);
align 16
serial_get:
pause
call serial_peek
test al,al
jz serial_get

mov dx,cx
in al,dx
ret

;void serial_put(word,byte);
align 16
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
align 16
BugCheck:

push rbp
mov rbp,rsp
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

mov rax,[rbp+8]	;ret addr
mov [rbp+0x10],rcx	;errcode
mov [rbp+0x18],rax	;rip

;mov rdx,rcx
mov ecx,0xFFFFFFFF
mov rdx,rsp

sub rsp,0x20

call dispatch_exception


.reboot:

mov dx,0x92
in al,dx
or al,1
out dx,al
hlt
jmp .reboot

align 16
__chkstk:
;probe every page

;r10 request stack top
;r11 current page

sub rsp,0x10
mov [rsp+8],r11
mov [rsp],r10
lea r10,[rsp+0x18]
mov r11,r10
sub r10,rax

and r11w,0xF000

.step:

mov byte [r11],0
sub r11,0x1000

cmp r11,r10
jae .step


mov r11,[rsp+8]
mov r10,[rsp]
add rsp,0x10
ret



;__C_specific_handler:


;xor rax,rax
;ret
