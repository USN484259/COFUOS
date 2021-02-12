[bits 64]

HIGHADDR equ 0xFFFF8000_00000000

IDT_BASE equ 0x0800
IDT_LIM equ 0x400	;64 entries

;WARNING: according to x64 calling convention 0x20 space on stack needed before calling C functions

extern dispatch_exception
extern dispatch_irq

global buildIDT
global fpu_init

global DR_match
global DR_get
global DR_set

global memset
global zeromemory
global memcpy

global bugcheck_raise
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
align 8, db 0xCC
%endmacro

align 16
ISR_exception:
%assign i 0
%rep 20
ISR_STUB i
%assign i i+1
%endrep

exception_entry:

;+98	SS
;+90	rsp
;+88	rflags
;+80	CS
;+78	rip
;+70	errcode	|	r15
;+68	exp#		|	r14
;+60	r13
;+58	r12
;+50	r11
;+48	r10
;+40	r9
;+38	r8
;+30	rbp
;+28	rdi
;+20	rsi
;+18	rbx
;+10	rdx
;+08	rcx
;+00	rax

sub rsp,(8*15 - 2*8 + 0x20)
mov [rsp+0x20+0x00],rax
mov [rsp+0x20+0x08],rcx
mov [rsp+0x20+0x10],rdx
mov [rsp+0x20+0x18],rbx
mov [rsp+0x20+0x20],rsi
mov [rsp+0x20+0x28],rdi
mov [rsp+0x20+0x30],rbp
mov rax,ISR_exception
mov rcx,[rsp+0x20+0x68]		;exp#
mov rdx,[rsp+0x20+0x70]		;errcode
mov [rsp+0x20+0x38],r8
mov [rsp+0x20+0x40],r9
mov [rsp+0x20+0x48],r10
mov [rsp+0x20+0x50],r11
lea rbp,[rsp+0x20]
sub rcx,rax
mov [rsp+0x20+0x58],r12
mov [rsp+0x20+0x60],r13
mov [rsp+0x20+0x68],r14		;overwrite exp#
mov [rsp+0x20+0x70],r15		;overwrite errorcode

shr rcx,3
test word [rsp+0x20+0x80],0x03	;CS
mov ax,ss
mov r8,rbp	;rbp points to context
jz .en_no_swap_gs
swapgs
.en_no_swap_gs:
mov ds,ax
mov es,ax

cld
call dispatch_exception

test word [rbp+0x80],0x03	;CS
mov ax,[rbp+0x98]	;SS
mov rsp,rbp
jz .ex_no_swap_gs
swapgs
.ex_no_swap_gs:
mov r15,[rbp+0x70]
mov r14,[rbp+0x68]
mov r13,[rbp+0x60]
mov r12,[rbp+0x58]
mov r11,[rsp+0x50]
mov r10,[rsp+0x48]
mov r9,[rsp+0x40]
mov r8,[rsp+0x38]
mov ds,ax
mov es,ax
mov rbp,[rsp+0x30]
mov rdi,[rsp+0x28]
mov rsi,[rsp+0x20]
mov rbx,[rsp+0x18]
mov rdx,[rsp+0x10]
mov rcx,[rsp+0x08]
mov rax,[rsp+0x00]
add rsp,(8*15)
iretq

align 16
ISR_irq:
%rep (IDT_LIM/0x10 - 0x20)
call near irq_entry
align 8, db 0xCC
%endrep

irq_entry:
;+40	SS
;+38	rsp
;+30	rflags
;+28	CS
;+20	rip
;+18	exp#

sub rsp,0x18
test word [rsp+0x28],0x03	;CS
mov [rsp],rbp
jz .en_no_swap_gs
swapgs
.en_no_swap_gs:
mov rbp,[gs:8]	;this_thread
mov [rbp+CONX_OFF+0x00],rax
mov [rbp+CONX_OFF+0x08],rcx
mov [rbp+CONX_OFF+0x10],rdx
mov [rbp+CONX_OFF+0x18],rbx
mov [rbp+CONX_OFF+0x20],rsi
mov [rbp+CONX_OFF+0x28],rdi
;	rbp @ [rsp]
mov rax,ISR_irq
mov rcx,[rsp]	;rbp
mov rdx,[rsp+0x18]	;exp#
mov [rbp+CONX_OFF+0x38],r8
mov [rbp+CONX_OFF+0x40],r9
mov [rbp+CONX_OFF+0x48],r10
mov [rbp+CONX_OFF+0x50],r11
mov [rbp+CONX_OFF+0x30],rcx	;rbp
lea rsi,[rsp+0x20]
lea rdi,[rbp+CONX_OFF+0x78]
sub rdx,rax
mov ecx,5
cld
mov ax,ss
mov [rbp+CONX_OFF+0x58],r12
mov [rbp+CONX_OFF+0x60],r13
mov [rbp+CONX_OFF+0x68],r14
mov [rbp+CONX_OFF+0x70],r15
shr edx,3
rep movsq
mov ds,ax
mov es,ax

mov ecx,edx		;id
call dispatch_irq

mov rdx,[gs:8]
xor rbx,rbx
cmp rdx,rbp
lea rdi,[rsp+0x20]
jz .no_switch
lea rsi,[rdx+CONX_OFF+0x78]
mov ecx,5
mov rbp,rdx
rep movsq
.no_switch:
add rsp,0x20
mov r15,[rbp+CONX_OFF+0x70]
mov r14,[rbp+CONX_OFF+0x68]
mov r13,[rbp+CONX_OFF+0x60]
mov r12,[rbp+CONX_OFF+0x58]
mov ax,[rsp+0x20]	;SS
test word [rsp+0x08],0x03	;CS
mov r11,[rbp+CONX_OFF+0x50]
mov r10,[rbp+CONX_OFF+0x48]
mov r9,[rbp+CONX_OFF+0x40]
mov r8,[rbp+CONX_OFF+0x38]
;rbp below
;zeroing context.rflags
;mark as invalid context
mov [rbp+CONX_OFF+0x88],rbx		;rflags, rbx == 0
jz .ex_no_swap_gs
swapgs
.ex_no_swap_gs:
mov ds,ax
mov es,ax
mov rdi,[rbp+CONX_OFF+0x28]
mov rsi,[rbp+CONX_OFF+0x20]
mov rbx,[rbp+CONX_OFF+0x18]
mov rdx,[rbp+CONX_OFF+0x10]
mov rcx,[rbp+CONX_OFF+0x08]
mov rax,[rbp+CONX_OFF+0x00]

mov rbp,[rbp+CONX_OFF+0x30]
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
fpu_init:
mov DWORD [rsp+8],0x1F80
fninit
ldmxcsr [rsp+8]
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
nop DWORD [eax + eax*1 + 00000000h]

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


align 16
bugcheck_raise:

;+98	SS
;+90	rsp
;+88	rflags
;+80	CS
;+78	rip		|	retaddr
;+70	r15		|	rflags	<-- rsp after pushfq
;+68	r14
;+60	r13
;+58	r12
;+50	r11
;+48	r10
;+40	r9
;+38	r8
;+30	rbp
;+28	rdi
;+20	rsi
;+18	rbx
;+10	rdx
;+08	rcx
;+00	rax

pushfq
cli
sub rsp,0x70+0x20
mov [rsp+0x20+0x00],rax
mov [rsp+0x20+0x08],rcx
mov [rsp+0x20+0x10],rdx
mov [rsp+0x20+0x18],rbx
mov [rsp+0x20+0x20],rsi
mov [rsp+0x20+0x28],rdi
mov [rsp+0x20+0x30],rbp
mov ax,ss
mov bx,cs
mov rdi,[rsp+0x20+0x70]		;rflags
lea rsi,[rsp+0x20+0x80]		;old rsp
mov [rsp+0x20+0x38],r8
mov [rsp+0x20+0x40],r9
mov [rsp+0x20+0x48],r10
mov [rsp+0x20+0x50],r11
mov [rsp+0x20+0x58],r12
mov [rsp+0x20+0x60],r13
mov [rsp+0x20+0x68],r14
mov [rsp+0x20+0x70],r15
;rip & retaddr at the same position, skip
mov [rsp+0x20+0x80],rbx		;cs
mov [rsp+0x20+0x88],rdi		;rflags
mov [rsp+0x20+0x90],rsi		;rsp
mov [rsp+0x20+0x98],rax		;ss



lea r8,[rsp+0x20]	;context
mov ecx,0xFFFFFFFF

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
