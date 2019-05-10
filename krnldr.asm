IA32_EFER equ 0xC0000080



TSS_BASE equ 0x1000
GDT_BASE equ 0x1200
GDT_LIM equ 0x200
MEMSCAN_BASE equ 0x1400

MEMBMP_BASE equ 0x20000
MEMBMP_LIM equ 0x80000


PG4D equ 0x18000


;	Memory layout
;	Virtual Address		Physical Address

;	000000	001000		000000	001000		GAP	NULL address
;	001000	002000		001000	002000		R	TSS & GDT & MEMSCAN
;	002000	00A000		002000	00A000		RW	avl	
;	00A000	010000		00A000	010000		RW	stacks
;	010000	018000		010000	018000		RX	loader code & data
;	018000	020000		018000	020000		RW	PG
;	020000	0A0000		020000	0A0000		RW	PMMBMP up to 16G
;	0A0000	100000		0A0000	100000		R	BIOS area
;	---- direct map ----

;	100000	400000		Video memory		W	3MB big enough ?

;						100000	MAXPMM	?	Allocable PMM












section code vstart=0x10000

[bits 16]

jmp entry

align 4

dd 'INFO'
cpuinfo dd 0
addrinfo:
db 36
db 32
dw 0

VBE_width dw 0
VBE_height dw 0
VBE_bpp dw 24
VBE_mode dw 0
VBE_addr dd 0
VBE_lim dd 0


;env here

align 16

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

gdt32_len equ ($-gdt32_base-1)

gdt64_base:
dd 0
dd 0000_0000_0010_0000_1001_1000_0000_0000_b	;sys CS

dd 0
dd 1001_0010_0000_0000_b	;sys DS

dd 0
dd 0000_0000_0010_0000_1111_1100_0000_0000_b	;user CS	confirming ?

dd 0
dd 1111_0010_0000_0000_b	;user DS


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



cmp ax,[es:VBE_width]
jb video_loop
cmp dx,[es:VBE_height]
jb video_loop

movzx cx,BYTE [ss:MEMSCAN_BASE+0x19]
cmp cx,[es:VBE_bpp]
jb video_loop


;save current video mode

mov di,VBE_width

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
mov bx,[VBE_mode]
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
mov ax,gdt32_len
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



zeromemory:		;edi target		ecx size align to DWORD
test cl,0011_b
jnz abort
xor eax,eax
shr ecx,2
cld
rep stosd

ret


abort:
hlt
jmp abort

align 4

strhello db 'HELLO',0
strworld db 'WORLD',0
strabort db 'ABORTED',0
align 4


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

mov [cpuinfo],ebx		;SMAP SMEP INVPCID	TODO

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

mov [addrinfo],ax	;MAXPHYADDR


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


;init GDT64
mov edi,GDT_BASE
mov ecx,GDT_LIM
call zeromemory

mov esi,gdt64_base
mov edi,GDT_BASE+8
mov ecx,8
rep movsd



;paging init
;NOTE shall init PMMBMP first

cld
xor ecx,ecx


mov edx,PG4D
mov edi,edx
mov cr3,edx
add edx,0x1000
mov eax,edx
mov al,7
stosd
mov [edi],ecx

mov edi,edx
add edx,0x1000
mov eax,edx
mov al,7
stosd
mov [edi],ecx

mov edi,edx
xor eax,eax
mov al,0x83		;set PAT here
stosd
mov [edi],ecx

;enable PG
mov eax,cr0
bts eax,31
mov cr0,eax

;NOTE GDT and TSS no longer write-able since now


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
mov rsp,0x10000
mov rbp,rsp






hlt



