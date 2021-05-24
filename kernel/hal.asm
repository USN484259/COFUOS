[bits 64]

HIGHADDR equ 0xFFFF8000_00000000

GDT_BASE equ (HIGHADDR+0x0600)
GDT_LIM equ 0x600	;96 entries
IDT_BASE equ (HIGHADDR+0x0C00)
IDT_LIM equ 0x400	;64 entries

SEG_USER_SS equ 0x2B

; NOTE: according to x64 calling convention, 0x20 space on stack needed before calling C functions

extern dispatch_exception
extern dispatch_irq
extern kernel_service

global ISR_exception
global ISR_irq

global service_entry
global service_exit

global DR_match
global DR_get
global DR_set

global bugcheck_raise

global keycode

section .text

; 0 ~ 7, 9, 15, 16, 18, 19 no errcode
; 8, 10 ~ 14, 17 has errcode

%macro ISR_STUB 1
%if %1 == 8 || %1 == 17 || (%1 >= 10 && %1 <= 14)
;
%else
push 0
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
mov ds,ax
mov es,ax
shr edx,3
rep movsq

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

;SYSCALL target
; rcx <-- rip
; r11 <-- rflags
;IF DF TF cleared
;TODO watch out NMI
service_entry:
swapgs
bt r11d,8	;TF
mov r10,[gs:8]
mov r11,rsp	;user rsp as context
mov rsp,[r10+KSP_OFF]
setc r10b
sti
sub rsp,0x40
mov [rsp+0x20],rcx	; user rip
shl r10d,8
mov rcx,rax
mov [rsp+0x28],r11	; user rsp
mov ax,ss
mov [rsp+0x30],r10d	; user rflags
mov ds,ax
mov es,ax
call kernel_service
cli
mov r11d,0x202
mov dx,SEG_USER_SS
mov rcx,[rsp+0x20]	; user rip
or r11w,[rsp+0x30]	; user rflags
mov rsp,[rsp+0x28]	; user rsp
swapgs
mov ds,dx
mov es,dx
o64 sysret
; rax as return value
; rflags should have IF set

align 16
; rcx entry
; rdx module_base | arg
; r8  commandline
; r9  stack_top
service_exit:
cli
mov ax,SEG_USER_SS
mov r11d,0x202	;IF
lea rsp,[r9 - 0x30]
swapgs
mov ds,ax
mov es,ax
o64 sysret

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


section .rdata
align 16
keycode:
incbin 'keycode.bin'