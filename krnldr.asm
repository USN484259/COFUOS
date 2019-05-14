IA32_EFER equ 0xC0000080


TSS_BASE equ 0x0000
TSS_LIM equ 0x200
GDT_BASE equ 0x0200
GDT_LIM equ 0x200	;64 entries
IDT_BASE equ 0x0400
IDT_LIM equ 0x400	;128 entries

SYSINFO_BASE equ 0x0800


MEMSCAN_BASE equ 0x1000



ISR_STK equ 0x02000

PL4T equ 0x03000

PDPT0 equ 0x4000
PDPT8 equ 0x5000

PDT0 equ 0x06000

PT0	equ 0x07000

PT_BASE equ 0x3000
PT_LEN equ (0x8000-PT_BASE)

;	physical Memory layout



;	000000		001000		R	TSS & GDT & IDT & sysinfo
;	001000		002000		RW	ISR stack
;	002000		003000		RX	loader		RW	3G PDT ?
;	003000		004000		RW	PL4T
;	004000		005000		RW	PDPT low
;	005000		006000		RW	PDPT high
;	006000		007000		RW	PDT
;	007000		008000		RW	PT
;	008000		0A0000		?	avl


;	100000		?			RW	PMMWMP







section code vstart=0x2000

[bits 16]

jmp entry


struc sysinfo
.sig	resq 1
.cpu	resd 1
.bus	resw 1
.port	resw 7
.VBE_bpp	resw 1
.VBE_mode	resw 1
.VBE_width	resw 1
.VBE_height	resw 1
.VBE_addr	resd 1
.VBE_lim	resd 1
.FAT_table	resd 1
.FAT_backup	resd 1
.FAT_cluster	resd 1
.FAT_data	resd 1

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
dd 0000_0000_0010_0000_1001_1000_0000_0000_b	;sys64 CS

dd 0
dd 1001_0010_0000_0000_b	;sys64 DS


dw 0xFFFF
dw 0
db 0
db 1111_1010_b
db 1100_1111_b
db 0		;user cs

dw 0xFFFF
dw 0
db 0
db 1111_0010_b
db 1100_1111_b
db 0		;user ds



dd 0
dd 0000_0000_0010_0000_1111_1000_0000_0000_b	;user64 CS

dd 0
dd 1111_0010_0000_0000_b	;user64 DS


dw TSS_LIM
dw TSS_BASE
dw 0x8900
dw 0
dq 0		;TSS



gdt64_len equ ($-gdt64_base)

GDT_TSS equ 7*8
GDT_SYS_CS equ 1*8
GDT_SYS_DS equ 2*8


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


;sysinfo setup
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


xor cx,cx
mov es,cx



mov edx,'VESA'
cmp [es:MEMSCAN_BASE+0x200],edx
jnz abort16

mov si,[es:MEMSCAN_BASE+0x200+0x0E]	;video modes offset
mov cx,[es:MEMSCAN_BASE+0x200+0x10]	;video modes segment


;	WARNING DS changed
mov ds,cx


video_loop:
lodsw	;next mode
mov cx,ax
mov bx,ax
inc ax
jz video_end



mov di,MEMSCAN_BASE

mov ax,0x4F01
int 0x10	;query video mode

xor dx,dx
mov es,dx

cmp ax,0x4F
jnz video_end


test BYTE [es:MEMSCAN_BASE],0x80
jz video_loop


mov cx,[es:MEMSCAN_BASE+0x12]
cmp cx,800
jb video_loop

mov dx,[es:MEMSCAN_BASE+0x14]
cmp dx,600
jb video_loop




cmp cx,[es:SYSINFO_BASE+sysinfo.VBE_width]
jb video_loop
cmp dx,[es:SYSINFO_BASE+sysinfo.VBE_height]
jb video_loop

movzx ax,BYTE [es:MEMSCAN_BASE+0x19]
cmp ax,[es:SYSINFO_BASE+sysinfo.VBE_bpp]
jb video_loop


;save current video mode

mov di,SYSINFO_BASE+sysinfo.VBE_bpp

stosw

xchg ax,bx
stosw

mov ax,cx
stosw

mov ax,dx
stosw

mul cx

shl edx,16
shr bx,3
mov dx,ax




;mov [es:VBE_width],ax
;mov [es:VBE_height],dx
;mov [es:VBE_bpp],bx

mov eax,[es:MEMSCAN_BASE+0x28]
stosd

movzx eax,bx
mul edx

;mov eax,[es:MEMSCAN_BASE+0x2C]
stosd


jmp video_loop

video_end:

push cs
pop ds

mov si,strvberr

mov bx,[es:SYSINFO_BASE+sysinfo.VBE_mode]
test bx,bx
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

xor cx,cx
mov ax,gdt32_base
mov dx,gdt32_len-1
push cx
push ax
push dx
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
mov esp,ISR_STK
mov ebp,esp		;stack frame






push 10_0000_0000_0000_0000_0010_b
popfd

;save ports
mov esi,0x0400
mov edi,SYSINFO_BASE+sysinfo.port
mov ecx,7
rep movsw

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
mov eax,ISR_STK
add edi,4
stosd


;paging init


mov edi,PT_BASE
mov ecx,PT_LEN
call zeromemory

mov edi,PL4T
xor ecx,ecx
mov ebx,PDPT0
mov edx,0x03
inc ecx
call map_page

mov edi,PL4T+0x800
mov ebx,PDPT8
inc ecx
call map_page


mov edi,PDPT0
mov ebx,PDT0
inc ecx
call map_page


mov edi,PDPT8
mov ebx,PDT0
inc ecx
call map_page


mov edi,PDT0
mov ebx,PT0
inc ecx
call map_page






mov edi,PT0
xor ebx,ebx
mov edx,0x80000003
mov cl,2
call map_page

mov ebx,0x2000
mov edx,0x01
inc ecx
call map_page

mov ebx,PT_BASE
mov edx,0x80000003
mov cl,PT_LEN>>12
call map_page



;TODO
;init PMMWMP here





mov eax,PL4T
mov cr3,eax

;enable PG
mov eax,cr0
bts eax,31
mov cr0,eax


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


jmp DWORD GDT_SYS_CS:LM_entry


map_page:	;edi PT		ebx PMM		ecx cnt		edx attrib

mov eax,ebx
xor ebx,ebx
mov al,dl

bt edx,31
rcr ebx,1

.st:

stosd
add eax,0x1000
mov [edi],ebx
add edi,4

loop .st

ret


[bits 64]

HIGHADDR equ 0xFFFF_8000_0000



align 16

LM_entry:


mov ax,GDT_SYS_DS
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

section codehigh vstart=0xFFFF8000_00002000+CODE64OFF

ldrstk:

TR_selector:
dw GDT_TSS

align 8

LM_high:

mov rdx,TR_selector
ltr [rdx]





hlt




align 16


BugCheck:

hlt
jmp BugCheck


;ReadSector dst,LBA,cnt
;TODO get ports from PCI
ReadSector:

push rdi


mov r9,rdx		;LBA
mov rax,r8		;cnt
mov rdi,rcx		;dst

mov dx,0x1F2
out dx,al

mov rax,r9
inc dx
out dx,al		;0x1F3

shr eax,8
inc dx
out dx,al		;0x1F4

shr eax,8
inc dx
out dx,al		;0x1F5

shr eax,8
inc dx
btc eax,4
or al,0xE0
out dx,al		;0x1F6

mov al,0x20
inc dx
out dx,al		;0x1F7

xor ecx,ecx
.wait:

inc ecx
jc BugCheck

pause

in al,dx
test al,0x80
jnz .wait
test al,0010_0001_b
jnz BugCheck
test al,8
jz .wait

mov rcx,r8
mov dx,0x1F0
shl rcx,8		;*256
rep insw

pop rdi

ret



;ReadFile dst,cluster
;return next cluster
ReadCluster:

push rbx
push rdx

xor rax,rax
mov r8d,128
xchg rdx,rax
mov rbx,rcx	;dst
div r8d

mov r9,HIGHADDR+SYSINFO_BASE+sysinfo.FAT_table

add eax,DWORD [r9]
xor r8,r8
push rdx	;remainder

;rcx dst
mov rdx,rax
inc r8
call ReadSector
xor rcx,rcx
pop rax
mov ecx,[rbx+4*rax]

mov r9,HIGHADDR+SYSINFO_BASE+sysinfo.FAT_cluster
pop rax

mov r8d,DWORD [r9]
xchg rcx,rbx

mul r8d
mov r9,HIGHADDR+SYSINFO_BASE+sysinfo.FAT_data

add eax,DWORD [r9]

sub eax,r8d
sub eax,r8d

mov rdx,rax
call ReadSector

xor rax,rax

cmp ebx,0x0FFFFFF8

cmovnb rax,rbx


pop rbx

ret


;GetFile filename,buf
;return cluster
GetFile:	

push rsi
push rdi
push rbx

mov r8,HIGHADDR+SYSINFO_BASE+sysinfo.FAT_data
xor rsi,rsi
mov rdi,rcx
mov esi,DWORD [r8]
mov rcx,rdx
mov rdx,rsi


call ReadCluster

mov r8,HIGHADDR+SYSINFO_BASE+sysinfo.FAT_cluster
mov ebx,DWORD [r8]
mov rdx,rdi		;filename
shl ebx,9
xor rcx,rcx
dec ebx			;cluster mask

xor rax,rax
.match:

cmp BYTE [rsi],0
jz .fail

mov rdi,rdx
mov cl,11

.name:
lodsb
xor al,[rdi]

test al,(~0x20)
jnz .next
inc rdi
loop .name


lodsb
test al,0x10
jnz .fail

add rsi,8
lodsw	;+0x14

shl eax,16
add rsi,4
lodsw

jmp .ret



.next:

add rsi,0x20
and rsi,0x1F

test esi,ebx
jnz .match

.fail:
xor rax,rax

.ret:


pop rbx
pop rdi
pop rsi

ret


