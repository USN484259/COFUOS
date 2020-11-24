IA32_EFER equ 0xC0000080
IA32_APIC_BASE equ 0x1B

HIGHADDR equ 0xFFFF_8000_0000_0000

IDT_BASE equ 0x0400
IDT_LIM equ 0x400	;64 entries

GDT_BASE equ 0x0800
GDT_LIM equ 0x100	;16 entries
TSS_BASE equ 0x0900
TSS_LIM equ 0x80

SYSINFO_BASE equ 0x0A00
MPCTRL_BASE equ 0x1000

PMMSCAN_BASE equ 0x1000

PAGE_SIZE equ 0x1000
SECTOR_SIZE equ 0x200

ISR_STK equ 0x03000

ISR_MP equ 0x9000

PL4T equ 0x03000

PDPT0 equ 0x4000
PDPT8 equ 0x5000

PDT0 equ 0x06000

PT0	equ 0x07000

PT_BASE equ 0x3000

;PT_PMM equ 0x8000

PT_LEN equ (0x8000-PT_BASE)

PMMQMP_PBASE equ 0x8000
;PMMQMP_PBASE equ 0x100000

;	physical Memory layout



;	000000		001000		RW	TSS & GDT & IDT & sysinfo
;	001000		002000		RW	PMMSCAN & MP sync area
;	002000		003000		RWX	loader & ISR stack
;	003000		004000		RW	PL4T
;	004000		005000		RW	PDPT low	
;	005000		006000		RW	PDPT high
;	006000		007000		RW	PDT
;	007000		008000		RW	PT
;	008000		009000		RW	PMMQMP within 2M
;------------------------	008000		009000		RW	PT for PMM
;	009000		010000		RW	MP ISR
;-------------direct map---------------------





struc sysinfo
.sig			resq 1
.PMM_avl_top	resq 1

.PMM_qmp_vbase	resq 1
.PMM_qmp_page	resd 1
.cpu			resd 1

.bus			resw 1
.port			resw 7

.VBE_bpp		resw 1
.VBE_mode		resw 1
.VBE_width		resw 1
.VBE_height		resw 1
.VBE_addr		resd 1
.VBE_lim		resd 1

.FAT_header		resd 1
.FAT_table		resd 1
.FAT_data		resd 1
.FAT_cluster	resd 1

.PE_filekrnl:
.krnlbase		resq 1
.AP_entry		resd 1
.MP_cnt			resw 1
				resw 1
.MP_id:
endstruc


struc mpctrl
.guard		resw 1
.owner		resw 1
.command	resw 1
.count		resw 1
endstruc




section code vstart=0x2000

[bits 16]



push cs
pop ds
cld

xor cx,cx
mov bx,0x7C00
mov es,cx


;	ds	->	cur code segment
;	es	->	flat address
cmp WORD [es:bx+0x200-2],0xAA55
jz near BSP_entry

xor dx,dx
mov bx,MPCTRL_BASE+mpctrl.guard
inc dx

AP_entry:
;acquire lock

xor ax,ax
pause
lock cmpxchg [es:bx],dx
jnz AP_entry

;inc WORD [es:bx+2]	;MP_cnt

mov ss,cx
mov sp,ISR_STK

mov di,SYSINFO_BASE+sysinfo.MP_cnt

cmp WORD [es:di],8
jb near AP_PE	;currently support less than 8 processors

xor ax,ax
lock mov [es:bx],ax

cli
.reset:
hlt
jmp .reset


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


tss64_base:

dd 0
dq HIGHADDR+ISR_STK
dq 0
dq 0
dq 0
dq HIGHADDR+ISR_MP+PAGE_SIZE*1
dq HIGHADDR+ISR_MP+PAGE_SIZE*2
dq HIGHADDR+ISR_MP+PAGE_SIZE*3
dq HIGHADDR+ISR_MP+PAGE_SIZE*4
dq HIGHADDR+ISR_MP+PAGE_SIZE*5
dq HIGHADDR+ISR_MP+PAGE_SIZE*6
dq HIGHADDR+ISR_MP+PAGE_SIZE*7
dq 0
dd 0

tss64_len equ ($-tss64_base)

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



set_PMM_top:	;si INFO	di PMMSCAN

mov eax,[es:di+0x10]
dec eax
jnz .end



mov eax,[es:di]
mov ecx,[es:di+4]

add eax,[es:di+8]
adc ecx,[es:di+0x0C]

cmp eax,[es:di]
jnz .nzero
cmp ecx,[es:di+4]
jnz .nzero

jmp .end

.nzero:
cmp ecx,[es:si+4]
jb .end
ja .found

cmp eax,[es:si]
jbe .end

.found:
mov [es:si],eax
mov [es:si+4],ecx

.end:

ret


BSP_entry:
mov ss,cx
mov sp,bx	;0x7C00

;	ds	->	cur code segment
;	es	->	flat address


mov si,strboot
call print16


;sysinfo setup
xor ax,ax
mov di,SYSINFO_BASE
mov cx,0x800/2
rep stosw

mov di,SYSINFO_BASE+sysinfo.sig

mov eax,'SYSI'
stosd
mov eax,'NFO'
stosd

mov di,SYSINFO_BASE+sysinfo.bus
mov ax,0x2024
stosw

mov di,SYSINFO_BASE+sysinfo.VBE_bpp
mov ax,24
stosw


mov si,0x7C00+0x200-0x10	;FAT_INFO from boot
mov di,SYSINFO_BASE+sysinfo.FAT_header
mov cx,7
es rep movsw



;detect VBE
mov di,PMMSCAN_BASE+0x200
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
cmp [es:PMMSCAN_BASE+0x200],edx
jnz abort16

mov si,[es:PMMSCAN_BASE+0x200+0x0E]	;video modes offset
mov cx,[es:PMMSCAN_BASE+0x200+0x10]	;video modes segment


;	WARNING DS changed
mov ds,cx


video_loop:
lodsw	;next mode
mov cx,ax
mov bx,ax
inc ax
jz .end



mov di,PMMSCAN_BASE

mov ax,0x4F01
int 0x10	;query video mode

xor dx,dx
mov es,dx

cmp ax,0x4F
jnz .end


test BYTE [es:PMMSCAN_BASE],0x80
jz video_loop


mov cx,[es:PMMSCAN_BASE+0x12]
cmp cx,800
jb video_loop

mov dx,[es:PMMSCAN_BASE+0x14]
cmp dx,600
jb video_loop




cmp cx,[es:SYSINFO_BASE+sysinfo.VBE_width]
jb video_loop
cmp dx,[es:SYSINFO_BASE+sysinfo.VBE_height]
jb video_loop

movzx ax,BYTE [es:PMMSCAN_BASE+0x19]
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

mov eax,[es:PMMSCAN_BASE+0x28]
stosd

movzx eax,bx
mul edx

;mov eax,[es:PMMSCAN_BASE+0x2C]
stosd


jmp video_loop

.end:

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
mov di,PMMSCAN_BASE
mov cx,0x800
rep stosw		;clear memscan area

mov si,SYSINFO_BASE+sysinfo.PMM_avl_top
mov di,PMMSCAN_BASE
mov ecx,ebx
mov edx,0x534D4150	;'SMAP'


;	QWORD		base
;	QWORD		length
;	DWORD		type
;	BYTE[4]		alignment

memscan_loop:
;mov edx,eax
mov cx,20
mov eax,0xE820
int 0x15			;BIOS memory detection
jc .end

;get PMM_avl_top here

mov edx,eax


call set_PMM_top


add di,24
test ebx,ebx
jnz memscan_loop

.end:

;align PMM_avl_top to 4K

and WORD [es:si],0xF000	;4K align


;notify BIOS of long mode
mov ax,0xEC00
mov bx,2
int 0x15

;A20 gate
mov dx,0x92
in al,dx
or al,2
out dx,al


AP_PE:


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

mov ecx,IA32_APIC_BASE
rdmsr

bt eax,8	;BSP
jnc near AP_LM


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
mov eax,7
cpuid

mov [SYSINFO_BASE+sysinfo.cpu],ebx		;SMEP (SMAP INVPCID ?)

cpuid0_basic:

mov eax,1
cpuid

;bt edx,16	;PAT	?
;jnc abort

bt edx,15	;cmovcc
jnc abort

bt edx,13	;PGE
jnc abort

bt edx,11	;SYSENTER
jnc abort

bt edx,9	;APIC
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
mov eax,0x80000008
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

mov esi,tss64_base
mov ecx,tss64_len/4
rep movsd


;zeroing IDT
mov edi,IDT_BASE
mov ecx,IDT_LIM
call zeromemory


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
mov edx,0x80000103
inc ecx
call map_page

mov ebx,0x1000
mov edx,0x8000011B	;CD WT
inc ecx
call map_page


mov ebx,0x2000
mov edx,0x103		;RWX
inc ecx
call map_page

mov ebx,PT_BASE
mov edx,0x8000011B	;CD WT
;mov edx,0x80000103
mov cl,PT_LEN>>12
call map_page


; up to 0x10000
mov ebx,PT_BASE+PT_LEN
mov edx,0x80000103
mov cl,0x10-((PT_BASE+PT_LEN)>>12)
call map_page



PMMQMP_init:

;init PDT here
;map PMMQMP to 0xFFFF8000_00200000

;mov edi,PDT0+8
;mov ebx,PT_PMM
;mov edx,0x80000003
;inc ecx
;call map_page

mov eax,[SYSINFO_BASE+sysinfo.PMM_avl_top]
mov ecx,[SYSINFO_BASE+sysinfo.PMM_avl_top+4]
test eax,0x001FFFFF
;test ecx,0x007FFFFF

jz .nalign

add eax,0x00200000
;add eax,0x00800000
adc ecx,0
.nalign:

shrd eax,ecx,21		; / 0x200 * 0x1000
;shrd ecx,eax,23		;/0x800*0x1000

;ecx PMM page
mov [SYSINFO_BASE+sysinfo.PMM_qmp_page],eax


mov edi,PT0+0x800
test ecx,ecx
mov ebx,PMMQMP_PBASE
mov edx,0x8000011B
jz .page_set

call BugCheck
int3

.page_set:
inc ecx
call map_page


mov edi,PMMQMP_PBASE
mov ecx,PAGE_SIZE
call zeromemory

mov esi,PMMSCAN_BASE
xor ebx,ebx		;addr


; PMMSCAN* block;
; qword* cur;
; qword* limit;

; for(;block && cur<limit;++cur){
	; qword addr=get_addr(cur)
	; if (addr < block->base)
		; continue
	
	; if (addr+PAGE_SIZE-1 > block->base + block->len){
		; ++block;
		; continue
	; }
	; if (!block->avl)
		; continue

	; *cur = avl

; }


.scan:
cmp edi,PMMQMP_PBASE+PAGE_SIZE
jae .end

xor edx,edx
mov eax,[esi]	;base
cmp edx,[esi+4]

jnz .big

cmp ebx,eax
jb .next

cmp edx,[esi+0x0C]
mov edx,ebx
jnz .big
add eax,[esi+8]	;len
jc .big
add edx,PAGE_SIZE-1
cmp edx,eax
jbe .range

.big:
add esi,24

.next:
add edi,8
add ebx,PAGE_SIZE
jmp .scan

.range:

mov edx,[esi+0x10]	;status
dec edx		;status
jz .avl
jns .next	;.rev
jmp .end

.avl:
add edi,4
mov eax,0x80000000
add ebx,PAGE_SIZE
stosd
jmp .scan


.end:


;set current state of PMM

mov ecx,0x10
xor edx,edx
mov edi,PMMQMP_PBASE

.cur_state:
mov eax,edx
inc edx
stosd
mov eax,8
stosd
loop .cur_state





AP_LM:

mov eax,cr4
bts eax,5	;PAE
bts eax,9	;OSFXSR
bts eax,10	;OSXMMEXCPT
mov cr4,eax

mov ecx,IA32_EFER
rdmsr
or ax,0000_1001_0000_0001_b		;NXE LME SCE
wrmsr



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
;xor eax,eax

;mov eax,0xFFFF8000
mov edx,GDT_BASE
mov ecx,(GDT_LIM-1)<<16
;push eax
push edx
push ecx
lgdt [esp+2]


jmp DWORD GDT_SYS_CS:LM_entry


map_page:	;edi PT		ebx PMM		ecx cnt		edx attrib

mov eax,ebx
xor ebx,ebx
or ax,dx
;mov al,dl

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

PMMQMP_VBASE equ HIGHADDR+0x00100000
KRNL_VBASE equ HIGHADDR+0x02000000

TMP_PT_VBASE equ HIGHADDR+0x10000
CMN_BUF_VBASE equ HIGHADDR+0x11000
PT_MAP_PT equ PT0+8*0x10
BUF_MAP_PT equ PT0+8*0x11

KRNL_STK_GUARD equ KRNL_VBASE-0x1000
KRNL_STK_TOP equ KRNL_STK_GUARD




;	virtual address of HIGHADDR
;	00000000	00010000	lowPMM
;	00010000	0001C000	VMG info
;	00010000	00011000	TMP_PT
;	00011000	?			CMN_BUF
;	00100000	?			PMMQMP
;	?			02000000	krnlstk for all MPs
;	02000000	?			KERNEL
;-------------------------------	20000000	40000000	PDTmap



struc peinfo
.headerpmm	resq 1
.vbase		resq 1
.entry		resd 1
.section	resw 1
.sec_attrib	resw 1
.sec_size	resd 1
.sec_off	resd 1
endstruc



align 16

LM_entry:

;reload GDT to HIGHADDR
mov rdx,HIGHADDR+GDT_BASE
mov rcx,(GDT_LIM-1)<<(16+32)

push rdx
push rcx

lgdt [rsp+6]



mov ax,GDT_SYS_DS
mov ds,ax
mov es,ax
mov fs,ax
mov gs,ax
mov ss,ax
mov rsp,HIGHADDR+ISR_STK
;mov rbp,rsp

mov ecx,8
mov eax,.cs_reload
push rcx
push rax

call qword [rsp]


; mov rax,.cs_reload
; push rcx
; push rax
; db 0x48
; retf

.cs_reload:

mov rdx,LM_high
mov rsp,HIGHADDR+ISR_STK
jmp rdx

align 4

CODE64OFF equ ($-$$)

section codehigh vstart=0xFFFF8000_00002000+CODE64OFF


TR_selector:
dw GDT_TSS

align 4

strkrnl db 'COFUOS',0x20,0x20,'SYS',0

align 8

LM_high:

mov rdx,TR_selector
ltr [rdx]


;unmap lowaddr
;mov rdi,HIGHADDR+PDPT0
;xor rax,rax
;stosq

;ebable PGE SMEP disable WRGSBASE

;mov rsi,HIGHADDR+SYSINFO_BASE+sysinfo.cpu
mov eax,[r12+sysinfo.cpu]
mov rdx,cr4
;lodsd
bt eax,7
jnc .nosmep
bts rdx,20	;SMEP

.nosmep:
bts rdx,7	;PGE
btr rdx,16	;FSGSBASE

mov cr4,rdx

;no need to reload CR3 since setting PGE flushs TLB
;mov rdx,cr3
;mov cr3,rdx

mov r12,HIGHADDR+SYSINFO_BASE


mov ecx,IA32_APIC_BASE
rdmsr

bt eax,8	;BSP
jc .BSP




mov ecx,[r12+sysinfo.AP_entry]
mov rdx,[r12+sysinfo.krnlbase]
mov rbp,rsp
add rdx,rcx
sub rsp,0x20
movzx rcx,WORD [r12+sysinfo.MP_cnt]
call rdx

jmp BugCheck


.BSP:



;mov rcx,HIGHADDR+SYSINFO_BASE+sysinfo.PMM_qmp_vbase
mov rax,PMMQMP_VBASE
mov [r12+sysinfo.PMM_qmp_vbase],rax

;mov rcx,HIGHADDR+SYSINFO_BASE+sysinfo.FAT_cluster
mov ax,[r12+sysinfo.FAT_cluster]
cmp ax,8
jbe .unmap_low

call BugCheck
int3
.unmap_low:





mov rbp,rsp
sub rsp,0x20

mov rsi,CMN_BUF_VBASE
mov rcx,rsi		;VA		;re-mapped to module_base
call PmmAlloc
mov rdx,rax
mov rcx,rsi
mov r8,0x80000000_00000003
;mov r12,rax		;r12 PMM
mov [rsp+peinfo.headerpmm],rax
call MapPage

find_krnl:
;xor rdx,rdx
xor r8,r8
mov rcx,rsi
mov edx,[r12+sysinfo.FAT_data]
inc r8
call ReadSector

mov rbx,SECTOR_SIZE
xor rcx,rcx
add rbx,rsi
;xor rax,rax

jmp .findfile

.fail:
call BugCheck
int3

.next:
add rsi,0x20
and sil,(~ 0x1F)

.findfile:

cmp rsi,rbx
jae .fail

cmp BYTE [rsi],0
jz .fail

mov rdi,strkrnl
mov cl,11

.name:
lodsb
xor al,[rdi]
test al,(~ 0x20)
jnz .next
inc rdi
loop .name

lodsb
test al,0x10
jnz .fail

add rsi,8
lodsw
shl eax,16
add rsi,4
lodsw

;rax firstcluster

test rax,rax
jz .fail

cmp eax,0x0FFFFFF8
jae .fail

lea rdi,[r12+sysinfo.PE_filekrnl]
;mov rdi,HIGHADDR+SYSINFO_BASE+sysinfo.PE_filekrnl
mov rsi,CMN_BUF_VBASE

.load_cluster:

xor rdx,rdx
mov ecx,128
stosd
xor r8,r8
div ecx
;mov r9,HIGHADDR+SYSINFO_BASE+sysinfo.FAT_table
mov rbx,rdx		;remainder
mov rcx,rsi
mov rdx,rax
inc r8
add edx,[r12+sysinfo.FAT_table]

call ReadSector

mov eax,[rsi+4*rbx]

test eax,eax
jz .fail

cmp eax,0x0FFFFFF8
jb .load_cluster



mov rcx,rsi
xor rdx,rdx
mov rdi,rsi
mov r8,SECTOR_SIZE
call ReadFile
add rdi,rax


;verify PE header
mov ax,[rsi]
;xor rdx,rdx
cmp ax,'MZ'
jnz .fail

mov edx,[rsi+0x3C]
add rsi,rdx

lodsd
cmp eax,'PE'
jnz .fail

lodsw	;machine
cmp ax,0x8664
jnz near .fail

lodsw	;section count
add rsi,0x0C
test ax,ax
mov [rsp+peinfo.section],ax
jz .fail

lodsw	;opt header size
cmp ax,0xF0
jnz .fail
lodsw	;Characteristics
bt ax,1		;executable
jnc .fail

;SYSTEM flag may be deprecated

;bt ax,12	;SYSTEM
;nc near .fail

lodsd
add rsi,0x0C
cmp ax,0x20B
jnz .fail

lodsd	;entrypoint RVA
mov [rsp+peinfo.entry],eax

lodsd
lodsq	;vbase
mov [rsp+peinfo.vbase],rax

lodsd	;section alignment
add rsi,0x18
test eax,(PAGE_SIZE-1)
jnz .fail
;lodsd	;file alignment
;add rsi,0x14
;mov [rsp+peinfo.filealign],ax	;needed ?

;xor rbx,rbx

lodsd	;header size
;xor r8,r8
cmp rsi,rdi
jae .fail

sub eax,SECTOR_SIZE
;mov rdx,r8
jbe .smallheader

mov rcx,rdi
mov edx,SECTOR_SIZE
mov r8d,eax
call ReadFile
add rdi,rax

.smallheader:


lodsd
lodsd	;(DLLCharacteristics<<16) | Subsystem
bt eax,24	;DEP
jnc .fail


;map vbase and stack
;rbx cluster size

lodsd	;stkrev

cmp eax,0x100000	;1M
mov edx,eax
ja .fail

lodsd	;stkrev high
test eax,eax
jnz .fail

lodsd	;stkcmt
cmp eax,edx
mov rdi,KRNL_STK_TOP
ja .fail
mov ebx,eax

lodsd	;stkcmt high

shr ebx,12	;rbx commit page count
test eax,eax
jnz .fail

cmp ebx,0x10	;16 pages
ja .fail


;no heap support
lodsq
test rax,rax
jnz .fail
lodsq
test rax,rax
jnz .fail

lodsd
lodsd	;datadir cnt

shl eax,3
add rsi,rax		;skip datadir

mov rbp,rdi

.makestk:


sub rdi,PAGE_SIZE
mov rcx,rdi
call PmmAlloc
mov r8,0x80000000_00000003
mov rdx,rax
mov rcx,rdi
call MapPage

dec ebx
jnz .makestk

;change rsp just before jmp to krnl


;map header to vbase and refresh QMP's record

mov rcx,[rsp+peinfo.vbase]
mov rdx,[rsp+peinfo.headerpmm]

mov rax,0x000F_FFFF_FFFF
mov r9,rcx
mov r10,rdx
shr r9,12	;VA index
mov r8,[r12+sysinfo.PMM_qmp_vbase]
and rax,r9
shr r10,12	;PMM index
xchg [r8+8*r10],rax
bt rax,35
jnc .fail

mov r8,0x80000000_00000001
call MapPage




;process sections

.section:

lodsq	;name
lodsd	;originsize
;xor rax,rax
mov rdi,[rsp+peinfo.vbase]
mov [rsp+peinfo.sec_size],eax
mov ecx,eax		;originsize

lodsd	;RVA
add rdi,rax		;section vbase

lodsd	;size
test eax,eax
mov [rsp+peinfo.sec_size],eax	;size in file
cmovnz ecx,eax




test cx,(PAGE_SIZE-1)
jz .section_aligned

add ecx,PAGE_SIZE

.section_aligned:
mov ebx,ecx		;size in memory

lodsd	;offset
add rsi,0x0C
;or ebx,eax		;rbx	( size<<32 ) | offset
mov [rsp+peinfo.sec_off],eax

lodsd	;attrib
shr eax,16
bt eax,9	;discard
jc .section_discard
mov [rsp+peinfo.sec_attrib],ax

mov r8,0x80000000_00000003
shr ebx,12		;page count
mov rcx,rdi		;vbase
mov rdx,rbx
call VirtualAlloc



mov r8d,[rsp+peinfo.sec_size]	;size
mov edx,[rsp+peinfo.sec_off]	;offset
test r8d,r8d
mov rcx,rdi		;dst
jz .nofile
call ReadFile

.nofile:

mov [rsp+peinfo.sec_size],ebx	;pagecount
mov dx,WORD [rsp+peinfo.sec_attrib]
xor rbx,rbx

bt dx,13	;X
jc .attrib_X
bts rbx,63

.attrib_X:

bt dx,15	;W

jnc .attrib_W
bts rbx,1

.attrib_W:

inc rbx

.section_attrib:
mov rcx,rdi
xor rdx,rdx
mov r8,rbx
call MapPage


add rdi,PAGE_SIZE
dec DWORD [rsp+peinfo.sec_size]
jnz .section_attrib

.section_discard:

dec WORD [rsp+peinfo.section]
jnz .section


;unmap CMN_BUF
mov rcx,CMN_BUF_VBASE
xor rdx,rdx
xor r8,r8
call MapPage

;xor rdx,rdx
mov rcx,[rsp+peinfo.vbase]
mov edx,[rsp+peinfo.entry]
mov rsp,rbp
add rdx,rcx
sub rsp,0x20


call rdx	;rcx module base

nop

BugCheck:
mov dx,0x92
in al,dx
or al,1
out dx,al
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
btr eax,4
or al,0xE0
out dx,al		;0x1F6

mov al,0x20
inc dx
out dx,al		;0x1F7

xor ecx,ecx
.wait:

inc ecx
jz .fail

pause

in al,dx
test al,0x80
jnz .wait
test al,0010_0001_b
jnz .fail
test al,8
jz .wait

movzx rcx,r8b
mov dx,0x1F0
shl rcx,8		;*256
rep insw

pop rdi

ret

.fail:
call BugCheck
int3




;ReadFile dst,off,size
;NOTE works but low efficiency
;return size
ReadFile:

test dx,0x1FF
jnz .fail

test r8w,0x1FF
jnz .fail

push r8		;size
shr rdx,9	;in sector
shr r8,9	;in sector

push rsi
push rdi
push rbx


mov rsi,rdx		;offset in sector
mov rdi,rcx		;dst
mov rbx,r8		;size in sector


.read:
xor rdx,rdx
;mov r9,HIGHADDR+SYSINFO_BASE+sysinfo.FAT_cluster
mov rax,rsi
div DWORD [r12+sysinfo.FAT_cluster]

mov rcx,rdx	;remainder
mov eax,[r12+sysinfo.PE_filekrnl+4*rax]

sub eax,2

mul DWORD [r12+sysinfo.FAT_cluster]

test edx,edx
mov edx,eax
jnz .fail


add edx,[r12+sysinfo.FAT_data]	;edx sector of target cluster
xor r8,r8
add edx,ecx		;target sector
inc r8
mov rcx,rdi
call ReadSector

add rdi,SECTOR_SIZE
inc rsi

dec rbx
jnz .read


pop rbx
pop rdi
pop rsi
pop rax		;size

ret

.fail:
call BugCheck
int3


; ==	0		->		reserved
; >	0		->		used
; ==	8-00000000		->		maps to nowhere
; >=	80000000-00000000		->		avl(linked)
; >	80000000-00000000		->		used,multiple-mapping


PmmAlloc:	;rcx VA

push rsi
push rdi
push rbx

mov rsi,[r12+sysinfo.PMM_qmp_vbase]
mov rdi,PAGE_SIZE
mov rbx,rsi
add rdi,rsi

.find:

lodsq
bt rax,63
jc .found

cmp rsi,rdi

jb .find

call BugCheck
int3

.found:
mov rax,0xF_FFFFFFFF
shr rcx,12
sub rsi,8
and rcx,rax
mov rax,rsi
mov [rsi],rcx
sub rax,rbx


pop rbx
pop rdi
pop rsi

shl rax,12-3

ret



;MapAddress VirtualAddr,PhysicalAddr,attrib
;PMM==0	change attrib
;attrib==0 unmap
MapPage:



mov eax,0x40000000		;1G

cmp ecx,eax
jae .fail

bt rcx,47
jnc .fail

push rbx
push rsi
push rdi

mov rbx,rdx		;PMM
;xor rsi,rsi

test r8,r8
cmovz rbx,r8

mov esi,ecx		;VA within 1G


mov rdi,HIGHADDR+PDT0
mov rax,rsi
;xor dil,dil
shr rax,21

lea rdi,[rdi+rax*8]

bt QWORD [rdi],0
jnc .newPT

mov rax,[rdi]
mov rdi,HIGHADDR+PT_MAP_PT
stosq

mov rdi,TMP_PT_VBASE
invlpg [rdi]

jmp .next

.fail:
call BugCheck


.newPT:

test rbx,rbx
jz .fail

test r8,r8
jz .end

push r8

;alloc new PT
mov rcx,0x8_00000000	;map to nowhere
call PmmAlloc
mov al,3
stosq

mov rdi,HIGHADDR+PT_MAP_PT
stosq

mov rdi,TMP_PT_VBASE
xor rax,rax
invlpg [rdi]
;mov rcx,PAGE_SIZE/8
stosq

mov rdi,TMP_PT_VBASE
pop r8

.next:
shr rsi,12
movzx rax,si
and ax,0x01FF

lea rdi,[rdi+rax*8]

test rbx,rbx
jnz .set

mov rbx,[rdi]

btr rbx,63
and bx,0xF000

test r8,r8

invlpg [rbx]

jnz .set

xor rax,rax
stosq
;PmmFree(rbx)

jmp .end

.set:

;ASSERT(rbx is valid PMM)
;ASSERT(r8 is valid attrib)
or rbx,r8
xchg [rdi],rbx

;PmmFree(rbx)


.end:

pop rdi
pop rsi
pop rbx

ret

;VirtualAlloc vbase,pagecount,attrib
VirtualAlloc:

push rsi
push rdi
push rbx

mov rsi,r8		;attrib
mov rdi,rcx		;vbase
mov rbx,rdx		;pagecount

.map:
mov rcx,rdi		;vaddr
call PmmAlloc
mov r8,rsi		;attrib
mov rcx,rdi		;vaddr
mov rdx,rax		;paddr
call MapPage

add rdi,PAGE_SIZE

dec rbx
jnz .map



pop rbx
pop rdi
pop rsi

ret


