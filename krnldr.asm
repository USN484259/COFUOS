IA32_EFER equ 0xC0000080



TSS_BASE equ 0x1000
TSS_LIM equ 0x200
GDT_BASE equ 0x1200
GDT_LIM equ 0x200	;64 entries
IDT_BASE equ 0x1400
IDT_LIM equ 0x400	;128 entries

SYSINFO_BASE equ 0x1800


MEMSCAN_BASE equ 0x2000



ISR_STK equ 0x04000
;guard page at 0x02000
GATE_STK equ 0x010000
;guard page at 0x0C000

PG4D equ 0x04000

PT0 equ 0x0A000
PDT0 equ 0x0B000
;	Memory layout
;	Virtual Address					Physical Address


;	000000000000	000000001000							GAP		NULL address
;									00000000	00001000	N		PDPT for krnl 512G layout fixed
;	000000001000	000000002000	00001000	00002000	R(W)	TSS & GDT & IDT & systeminfo
;	000000002000	000000004000	00002000	00004000	RW		ISR stack & PMMSCAN

;	000000004000	000000005000	00004000	00005000	RW		PG4D
;	000000005000	000000006000	00005000	00006000	RW		PDPT for krnl at high
;	000000006000	000000007000	00006000	00007000	RW		PDT for krnl at 2G
;	000000007000	000000008000	00007000	00008000	RW		PDT for krnl at 3G
;	000000008000	00000000A000	00008000	0000A000	RW		PT for video memory
;	00000000A000	00000000B000	0000A000	0000B000	RW		first PT for krnl
;	00000000B000	00000000C000							GAP		guard page
;									0000B000	0000C000	N		first PDT for krnl	1G layout fixed reused at high
;	00000000C000	000000010000	0000C000	00010000	RW		GATE stack
;	800000010000	800000020000	00010000	00020000	X		ldr code	avlPMM
;	?								00020000	000A0000	?		avlPMM
;									000A0000	00100000	N		BIOS area
;	000000004000	000000010000	?						RW		common PMM map area
;	?								00100000	MAXPMM		?		avlPMM
;	000000010000	000080000000	(allocated)				?		user area
;	000080000000	000100000000	(allocated)				RX		32bit krnl proxy
;	000100000000	800000000000	(allocated)				?		user area
;	800000000000   1000000000000	(allocated)				?		krnl area





section code vstart=0x10000

[bits 16]

jmp entry


struc sysinfo
.sig	resq 1
.cpu	resd 1
.bus	resw 1
.port	resw 7
.VBE_width	resw 1
.VBE_height	resw 1
.VBE_bpp	resw 1
.VBE_mode	resw 1
.VBE_addr	resd 1
.VBE_lim	resd 1

endstruc

align 4

strboot db 'COFUOS Loading ...',0
align 4
strvberr db 'No proper video mode',0

align 16

gdt32_base:
dq 0	;first empty entrance

dw 0xFFFF
dw 0
db 0	;base addr
db 1001_1010_b
db 1100_1111_b
db 0		;sys cs

dw 0xFFFF
dw 0
db 0
db 1001_0010_b
db 1100_1111_b
db 0		;sys ds


dq 0	;end of GDT

gdt32_len equ ($-gdt32_base)

gdt64_base:
dd 0
dd 0000_0000_0010_0000_1001_1000_0000_0000_b	;sys CS

dd 0
dd 1001_0010_0000_0000_b	;sys DS

dd 0
dd 0000_0000_0010_0000_1111_1100_0000_0000_b	;user CS	confirming ?

dd 0
dd 1111_0010_0000_0000_b	;user DS

dw TSS_LIM
dw TSS_BASE
dw 0x8900
dw 0
dq 0		;TSS

gdt64_len equ ($-gdt64_base)

align 16


print16:

mov ax,0x0300
xor bx,bx
int 0x10

xor cx,cx
mov bp,si
not cx
.cnt:
lodsb
inc cx
test al,al
jnz .cnt

push es
push ds
mov bx,0x0F
pop es

mov ax,0x1300
int 0x10

pop es

inc dh
xor bx,bx
mov ax,0x200
int 0x10

ret




abort16:
call print16
sti
.hlt:
hlt
jmp .hlt


entry:

push cs
pop ds
cld

xor cx,cx
mov es,cx
mov ss,cx
mov sp,0x7c00

;	ds	->	cur code segment
;	es	->	flat address


mov si,strboot
call print16

xor ax,ax
mov di,SYSINFO_BASE
mov cx,0x800/2
rep stosw

mov di,SYSINFO_BASE

mov eax,'SYSI'
stosd
mov eax,'NFO'
stosd

add di,4
mov ax,0x2024
stosw

add di,6
mov ax,24
stosw





;detect VBE
mov di,MEMSCAN_BASE+0x200
mov ecx,'VBE2'
mov [di],ecx
mov ax,0x4F00
int 0x10
cmp ax,0x4F
mov si,strvberr
jnz abort16


;xor cx,cx
;mov di,MEMSCAN_BASE
;mov es,cx

;TRICK	ss is 0


mov edx,'VESA'
cmp [ss:MEMSCAN_BASE+0x200],edx
jnz abort16

mov si,[ss:MEMSCAN_BASE+0x200+0x0E]	;video modes offset
mov cx,[ss:MEMSCAN_BASE+0x200+0x10]	;video modes segment

;WARNING	DS changed !!!

mov ds,cx

video_loop:
lodsw	;next mode
mov cx,ax
mov bx,ax
inc ax
jz video_end


xor dx,dx
mov di,MEMSCAN_BASE
mov es,dx


mov ax,0x4F01
int 0x10	;query video mode


cmp ax,0x4F
jnz video_end

;TRICK	ss is 0

test BYTE [ss:MEMSCAN_BASE],0x80
jz video_loop


mov ax,[ss:MEMSCAN_BASE+0x12]
cmp ax,800
jb video_loop

mov dx,[ss:MEMSCAN_BASE+0x14]
cmp dx,600
jb video_loop

;WARNING	ES changed

mov cx,0x1000
mov es,cx



cmp ax,[es:SYSINFO_BASE+sysinfo.VBE_width]
jb video_loop
cmp dx,[es:SYSINFO_BASE+sysinfo.VBE_height]
jb video_loop

movzx cx,BYTE [ss:MEMSCAN_BASE+0x19]
cmp cx,[es:SYSINFO_BASE+sysinfo.VBE_bpp]
jb video_loop


;save current video mode

mov di,SYSINFO_BASE+sysinfo.VBE_width

stosw
mov ax,dx
stosw
mov ax,cx
stosw
mov ax,bx
stosw
;mov [es:VBE_width],ax
;mov [es:VBE_height],dx
;mov [es:VBE_bpp],bx

mov eax,[ss:MEMSCAN_BASE+0x28]
stosd
mov eax,[ss:MEMSCAN_BASE+0x2C]
stosd


jmp video_loop

video_end:


mov cx,0x1000
xor dx,dx
mov ds,cx
mov es,dx
mov si,strvberr

;TODO check if resolution is set
mov bx,[SYSINFO_BASE+sysinfo.VBE_mode]
cmp dx,bx
jz abort16
mov ax,0x4F02
or bx,0x4000	;LFB enable
int 0x10
cmp ax,0x4F
jnz abort16



xor ebx,ebx

xor ax,ax
mov di,MEMSCAN_BASE
mov cx,0x800
rep stosw		;clear memscan area

mov di,MEMSCAN_BASE
mov ecx,ebx
mov eax,0x534D4150	;'SMAP'

memscan_loop:
mov edx,eax
mov cx,20
mov eax,0xE820
int 0x15			;BIOS memory detection
jc memscan_end
xor edx,edx
add di,32
cmp ebx,edx
jnz memscan_loop

;	QWORD		base
;	QWORD		length
;	DWORD		type
;	BYTE[12]	alignment



memscan_end:



;mov ax,0x0200
;xor bx,bx
;mov dx,0x1900
;int 0x10	;hide cursor


mov dx,0x92
in al,dx
or al,2
out dx,al	;A20 gate

xor ax,ax
inc ax
push ax
mov ax,gdt32_base
push ax
mov ax,gdt32_len-1
push ax
mov di,sp
lgdt [ss:di]

;	sp+0	WORD length-1
;	sp+2	DWORD linear base





;pg cd nw AM WP NE ET ts em MP PE
mov eax,101_0000_0000_0011_0011_b
mov cr0,eax

jmp DWORD 0x0008:pe_entry	;sys cs

align 4

[bits 32]

abort:
hlt
jmp abort


zeromemory:		;edi target		ecx size align to DWORD
push edi

test cl,0011_b
jnz abort
xor eax,eax
shr ecx,2
cld
rep stosd

pop edi
ret


pe_entry:

;reload all segments
mov ax,0x10
mov ds,ax
mov es,ax
mov fs,ax
mov gs,ax
mov ss,ax
mov esp,0x10000
mov ebp,esp		;stack frame


push 10_0000_0000_0000_0000_0010_b
popfd

nop

;check cpuid
pushfd
pop edx
test edx,1<<21
jz abort


xor eax,eax
cpuid

cmp al,1
jb abort


cmp al,7
jb cpuid0_basic

xor ecx,ecx
mov al,7
cpuid

mov [SYSINFO_BASE+sysinfo.cpu],ebx		;SMAP SMEP INVPCID	TODO

cpuid0_basic:

mov eax,1
cpuid

bt edx,16	;PAT	TODO
jnc abort

bt edx,13	;PGE	TODO
jnc abort

bt edx,6	;PAE
jnc abort

bt edx,5	;MSR
jnc abort


mov eax,0x80000000
cpuid

cmp al,1
jb abort


cmp al,8
jb cpuid8_basic
mov al,8
cpuid

mov [SYSINFO_BASE+sysinfo.bus],ax	;MAXPHYADDR


cpuid8_basic:

mov eax,0x80000001
cpuid

bt edx,29	;long mode
jnc abort

bt edx,20	;NX
jnc abort

;TODO  test all necessary functionalities

;enable PAE
mov eax,cr4
bts eax,5
mov cr4,eax

mov ecx,IA32_EFER
rdmsr
or ax,0000_1001_0000_0001_b		;NXE LME SCE
wrmsr


;save ports
mov esi,0x0400
mov edi,SYSINFO_BASE+sysinfo.port
mov ecx,7
rep movsw


;init GDT64
mov edi,GDT_BASE
mov ecx,GDT_LIM
call zeromemory

mov esi,gdt64_base
mov edi,GDT_BASE+8
mov ecx,gdt64_len/4
rep movsd

;init TSS
mov edi,TSS_BASE
mov ecx,TSS_LIM
call zeromemory
mov eax,GATE_STK
add edi,4
stosd
mov eax,ISR_STK
add edi,0x20
stosd


;paging init

xor edi,edi
mov ecx,0x1000
call zeromemory
mov eax,PDT0 | 3
stosd


mov edi,PG4D
mov ecx,(0xC000-PG4D)
call zeromemory

xor ecx,ecx

mov edx,edi
mov cl,3
mov cr3,edx
mov [edi],ecx



add edx,0x1000
add edi,0x0800
mov eax,edx

mov al,3
stosd

mov edi,edx
mov eax,PDT0 | 3
stosd

;TODO set video memory here



mov edx,PT0

mov edi,PDT0
mov eax,edx
mov al,3
stosd

;set pages of first 2MB
xor eax,eax
mov edi,edx	;PT0
stosd
stosd

mov ax,0x1003
mov edx,0x80000000

mov ecx,0x0F

PT0_loop1:
stosd
add eax,0x1000
mov [edi],edx
add edi,4

loop PT0_loop1

mov ebx,edi

btc eax,1
mov ecx,0x10

PT0_loop2:

stosd
add eax,0x1000
add edi,4

loop PT0_loop2

;clear guard page
xor eax,eax
sub ebx,(4+1)*8
stosd


;enable PG
mov eax,cr0
bts eax,31
mov cr0,eax

;NOTE GDT IDT TSS and SYSINFO no longer write-able since now

;test LMA

mov ecx,IA32_EFER
rdmsr
bt eax,10
jnc abort


;load GDTR and jmp to 64-bit mode
xor eax,eax
mov edx,GDT_BASE
mov ecx,(GDT_LIM-1)<<16
push eax
push edx
push ecx
lgdt [esp+2]


jmp DWORD 0x08:LM_entry


[bits 64]

align 16

LM_entry:


mov ax,0x10
mov ds,ax
mov es,ax
mov fs,ax
mov gs,ax
mov ss,ax
mov rsp,ldrstk
mov rbp,rsp

mov rdx,LM_high

jmp rdx

align 16

CODE64OFF equ ($-$$)

section codehigh vstart=0xFFFF8000_00010000+CODE64OFF

ldrstk:

TR_selector:
dw 5*8

align 8

LM_high:

mov rax,TR_selector
ltr [rax]




hlt



