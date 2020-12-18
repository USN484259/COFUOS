IA32_EFER equ 0xC0000080
IA32_APIC_BASE equ 0x1B

HIGHADDR equ 0xFFFF_8000_0000_0000

GDT_BASE equ 0x0600
GDT_LIM equ 0x100	;16 entries
TSS_BASE equ 0x0700
TSS_LIM equ 0x80

SYSINFO_BASE equ 0x0C00
SYSINFO_LEN equ (0x1000-SYSINFO_BASE)

IDT_BASE equ 0x0800
IDT_LIM equ 0x400	;64 entries

PAGE_SIZE equ 0x1000
SECTOR_SIZE equ 0x200

ISR_STK equ 0x02000

PL4T equ 0x03000
PDPT0 equ 0x4000
PDPT8 equ 0x5000
PDT0 equ 0x06000
PT0	equ 0x07000
PT_KRNL equ 0x8000
PT_MAP equ 0x9000

PT_BASE equ PL4T
PT_LEN equ (0xA000-PT_BASE)

PMMSCAN_BASE equ 0x1000
PMMSCAN_LEN equ 0x0F00

;	physical Memory layout

;	000000		001000		RW	TSS & GDT & IDT & sysinfo
;	001000		002000		RW	PMMSCAN & VBE_scan & ISR stack
;	002000		003000		RX	loader & MP entry
;	003000		004000		RW	PL4T
;	004000		005000		RW	PDPT low
;	005000		006000		RW	PDPT high
;	006000		007000		RW	PDT
;	007000		008000		RW	PT0
;	008000		009000		RW	krnl PT
;	009000		00A000		RW	mapper PT
;	00A000		010000		RW	avl
;-------------direct map---------------------
;	010000		?			?	kernel pages


struc sysinfo
.sig			resq 1
.ACPI_RSDT		resq 1

.PMM_avl_top	resq 1
.kernel_page	resd 1
.cpu			resd 1

.bus			resw 1
.port			resw 7

.VBE_mode		resw 1
.VBE_scanline	resw 1
.VBE_width		resw 1
.VBE_height		resw 1
.VBE_bpp		resw 1
				resw 1
.VBE_addr		resd 1

.FAT_header		resd 1
.FAT_table		resd 1
.FAT_data		resd 1
.FAT_cluster	resd 1

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
jmp near BSP_entry

align 4

strboot db 'COFUOS Loading ...',0
align 4
strvberr db 'No proper video mode',0
align 4
strnoacpi db 'ACPI not found',0
align 4
strnopci db 'PCI not found',0

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
dq 0
dq 0
dq 0
dq 0
dq 0
dq 0
dq 0
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
push cs
pop ds
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


ACPI_get:
lodsd
cmp eax,0x20445352
jnz .next

lodsd
cmp eax,0x20525450
jnz .next

;checksum
mov cx,0x0C
mov dl,0x1F
.checksum_1:
lodsb
add dl,al
loop .checksum_1
test dl,dl
jnz .next

cmp [si-5],al	;al == 0
jnz .v2
;ACPI version 1.0
mov eax,[si-4]
mov [es:SYSINFO_BASE+sysinfo.ACPI_RSDT],eax
stc
ret

.v2:
xor dx,dx
mov cx,0x10
.checksum_2:
lodsb
add dl,al
loop .checksum_2
test dl,dl
jnz .next
;ACPI version >= 2.0
;&XSDT should lower than (1 << 56)
cmp [si-0x05],cl	;cx == 0
jnz .next
;get XSDT
mov eax,[si-0x0C]
mov edx,[si-0x08]
mov [es:SYSINFO_BASE+sysinfo.ACPI_RSDT],eax
mov [es:SYSINFO_BASE+sysinfo.ACPI_RSDT+4],edx

dec cx	;cl == 0xFF, indicate XSDT
mov [es:SYSINFO_BASE+sysinfo.ACPI_RSDT+7],cl
stc
ret

.next:
clc
ret


BSP_entry:
mov ss,cx
mov sp,ISR_STK

;	ds	->	cur code segment
;	es	->	flat address


mov si,strboot
call print16


;sysinfo setup
xor ax,ax
mov di,SYSINFO_BASE
mov cx,SYSINFO_LEN/2
rep stosw

mov di,SYSINFO_BASE+sysinfo.sig

mov eax,'SYSI'
stosd
mov eax,'NFO'
stosd

mov WORD [es:SYSINFO_BASE+sysinfo.bus],0x2024
mov WORD [es:SYSINFO_BASE+sysinfo.VBE_bpp],24



mov si,0x7C00+0x200-0x10	;FAT_INFO from boot
mov di,SYSINFO_BASE+sysinfo.FAT_header
mov cx,7
es rep movsw

PCI_scan:
mov ax,0xB101
int 0x1A
jc .fail
and al,1
cmp edx,0x20494350
jnz .fail
cmp ax,1
jz .end
.fail:
mov si,strnopci
jmp abort16

.end:

ACPI_scan:

;search RSDP in EBDA
mov ax,[es:0x040E]
xor di,di
mov ds,ax
.search_ebda:
mov si,di
call ACPI_get
jc .end
add di,0x10
cmp di,0x400
jb .search_ebda

;search RDSP in BIOS ROM
mov ax,0xE000
xor di,di
mov ds,ax
.search_rom:
mov si,di
call ACPI_get
jc .end
add di,0x10
jnc .search_rom
; di overflow, check ds
mov ax,ds
add ax,0x1000
jc .fail	;ds overflow, not found
mov ds,ax
jmp .search_rom

.fail:
mov si,strnoacpi
jmp abort16

.end:
;NOTE DS not restored : vbe_scan changes DS again

vbe_scan:
;detect VBE
mov di,PMMSCAN_BASE+0x200
mov ecx,'VBE2'
mov [di],ecx
mov ax,0x4F00
int 0x10
cmp ax,0x4F
mov si,strvberr
jnz abort16

mov edx,'VESA'
cmp [es:PMMSCAN_BASE+0x200],edx
jnz abort16

mov si,[es:PMMSCAN_BASE+0x200+0x0E]	;video modes offset
mov cx,[es:PMMSCAN_BASE+0x200+0x10]	;video modes segment


;	WARNING DS changed
mov ds,cx


.video_loop:
lodsw	;next mode
mov cx,ax
mov bx,ax
inc ax
jz NEAR .end

mov di,PMMSCAN_BASE

mov ax,0x4F01
int 0x10	;query video mode

xor dx,dx
mov es,dx

cmp ax,0x4F
jnz .end


test BYTE [es:PMMSCAN_BASE],0x80
jz .video_loop


mov cx,[es:PMMSCAN_BASE+0x12]
cmp cx,800
jb .video_loop

mov dx,[es:PMMSCAN_BASE+0x14]
cmp dx,600
jb .video_loop

cmp cx,[es:SYSINFO_BASE+sysinfo.VBE_width]
jb .video_loop
cmp dx,[es:SYSINFO_BASE+sysinfo.VBE_height]
jb .video_loop

movzx ax,BYTE [es:PMMSCAN_BASE+0x19]
cmp ax,[es:SYSINFO_BASE+sysinfo.VBE_bpp]
jb .video_loop

;save current video mode

mov di,SYSINFO_BASE+sysinfo.VBE_mode
mov [es:di],bx
mov [es:di + 4],cx
mov [es:di + 6],dx
mov [es:di + 8],ax

mov dx,[es:PMMSCAN_BASE+0x10]
mov cx,[es:PMMSCAN_BASE+0x200+0x04]
sub cx,0x0300
js .nvbe3
mov dx,[es:PMMSCAN_BASE+0x32]
.nvbe3:
mov [es:di + 2],dx

mov ax,[es:PMMSCAN_BASE+0x28]
add di,0x0C
stosw
mov ax,[es:PMMSCAN_BASE+0x2A]
stosw

jmp .video_loop

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


mem_scan:
xor ebx,ebx

xor ax,ax
mov di,PMMSCAN_BASE
mov cx,PMMSCAN_LEN / 2
rep stosw		;clear memscan area

mov si,SYSINFO_BASE+sysinfo.PMM_avl_top
mov di,PMMSCAN_BASE
mov ecx,ebx
mov edx,0x534D4150	;'SMAP'


;	QWORD		base
;	QWORD		length
;	DWORD		type
;	BYTE[4]		alignment

.memscan_loop:
;mov edx,eax
mov cx,20
mov eax,0xE820
int 0x15			;BIOS memory detection
jc .end

;si == PMM_avl_top

mov edx,eax


call set_PMM_top


add di,24
test ebx,ebx
jnz .memscan_loop

.end:

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

;paging init
mov edi,PT_BASE
mov ecx,PT_LEN
call zeromemory

;build PT structure
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

mov edi,PDT0 + 0x08	;PDT[1] -> PT_MAP
mov ebx,PT_MAP
inc ecx
call map_page

;direct map boot area
mov edi,PT0
xor ebx,ebx
mov edx,0x80000103
mov cl,2
call map_page

mov ebx,0x2000
mov edx,0x101		;RX
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

GAP_VBASE equ HIGHADDR+0x00010000
PMMBMP_VBASE equ HIGHADDR+0x00400000
PT_MAP_VBASE equ HIGHADDR+0x00200000
CMN_BUF_VBASE equ HIGHADDR+0xE000
PT_MAP_TABLE equ HIGHADDR+PT_MAP
FAT_KRNL_CLUSTER equ HIGHADDR+0xC000
KRNL_STK_TOP equ HIGHADDR+0x001FF000

;	virtual address of HIGHADDR
;	00000000	0000C000	boot_area
;	0000C000	0000E000	KRNL_FAT_cluster
;	0000E000	00010000	common buffer
;	00010000	?			GAP
;	?			001FF000	KRNL_STK
;	001FF000	00200000	stack_guard
;	00200000	00400000	MAP_VIEW
;	00400000	00E00000<	PMMBMP
;	00E00000<	01000000	GAP
;	01000000>	?			KERNEL


struc peinfo
.headerpmm		resq 1
.vbase			resq 1
.entry			resd 1
.section		resw 1
.sec_attrib		resw 1
.sec_vbase		resq 1
.sec_mem_size	resd 1
.sec_page_count	resd 1
.sec_file_size	resd 1
.sec_file_off	resd 1
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

mov ecx,8
mov eax,.cs_reload
push rcx
push rax

call qword [rsp]

.cs_reload:

mov rdx,LM_high
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
mov r12,HIGHADDR+SYSINFO_BASE

;load TR
mov rdx,TR_selector
ltr [rdx]

;init IDT
mov rdi,HIGHADDR+IDT_BASE
xor rax,rax
mov rdx,(IDT_LIM-1) << 48
push rdi
mov ecx,IDT_LIM/8
push rdx
rep stosq
lidt [rsp+6]

;unmap LOWADDR
xor rax,rax
mov rdi,HIGHADDR+PDPT0
stosq
mov ecx,0x10
.flush_low:
invlpg [rax]
add rax,PAGE_SIZE
loop .flush_low

;ebable PGE SMEP disable WRGSBASE

;mov rsi,HIGHADDR+SYSINFO_BASE+sysinfo.cpu
mov eax,[r12+sysinfo.cpu]
mov rdx,cr4
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

mov ax,[r12+sysinfo.FAT_cluster]
cmp ax,8
jbe .cluster_fine
call BugCheck
int3

.cluster_fine:
sub rsp,0x40

call PmmAlloc
mov rdx,rax
mov rcx,GAP_VBASE
mov r8,0x80000000_00000003
mov [rsp+peinfo.headerpmm],rax
call MapPage

find_krnl:
mov rsi,CMN_BUF_VBASE
xor r8,r8
mov rcx,rsi
mov edx,[r12+sysinfo.FAT_data]
inc r8
call ReadSector

mov rbx,SECTOR_SIZE
xor rcx,rcx
add rbx,rsi

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
mov edx,[rsi]
;eax firstcluster
;edx filesize

cmp edx,0x70000
ja .fail	; over 448k, possilby overwrite BIOS

test eax,eax
jz .fail

cmp eax,0x0FFFFFF8
jae .fail

mov rdi,FAT_KRNL_CLUSTER
mov rsi,CMN_BUF_VBASE

.load_cluster:
xor r8,r8
mov ebx,eax
stosd	;save cluster number
mov edx,[r12+sysinfo.FAT_table]
and ebx,0x7F	; % 128
shr eax,7		; / 128
mov rcx,rsi		;CMN_BUF_VBASE
add edx,eax
inc r8
call ReadSector

mov eax,[rsi+4*rbx]

test eax,eax
jz .fail

cmp eax,0x0FFFFFF8
jb .load_cluster

mov rsi,GAP_VBASE

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

lodsd
add rsi,0x0C
cmp ax,0x20B	;Magic for 64-bit executable
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

lodsd	;header size
cmp rsi,rdi
jae .fail

cmp eax,SECTOR_SIZE
jbe .smallheader

dec eax
and ax,(~0x1FF)		;eax -> header_size_sector_aligned - SECTOR_SIZE
cmp eax,(PAGE_SIZE-SECTOR_SIZE)
ja .fail

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


;allocate stack

lodsd	;stk_reserved.low

cmp eax,0x100000	;1M
mov edx,eax
ja .fail

lodsd	;stk_reserved.high
test eax,eax
jnz .fail

lodsd	;stk_commit.low
cmp eax,edx
mov rdi,KRNL_STK_TOP
ja .fail
mov ebx,eax	;rbx commit page count

lodsd	;stk_commit.high
shr ebx,12	;rbx commit size
test eax,eax
jnz .fail


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
call PmmAlloc
mov r8,0x80000000_00000103
mov rdx,rax
mov rcx,rdi
call MapPage

dec ebx
jnz .makestk

;change rsp just before jmp to krnl

;build PT for kernel
mov rbx,[rsp+peinfo.vbase]
mov rax,HIGHADDR+0x01000000
mov rdx,HIGHADDR+PDT0
cmp rbx,rax
mov ecx,ebx
jb .fail
mov eax,0x00000003
shr ecx,21	;2MB, qword
or ax,PT_KRNL
cmp ecx,PAGE_SIZE
lea rdi,[rdx + 8*rcx]
jae .fail
stosq

;map header to vbase

mov rcx,rbx		;[rsp+peinfo.vbase]
mov rdx,[rsp+peinfo.headerpmm]
mov r8,0x80000000_00000101	;R
call MapPage


;process sections
;rsi -> section[0]
.section:

lodsq	;name
lodsd	;originsize
;xor rax,rax
mov rdi,[rsp+peinfo.vbase]
mov ebx,eax
mov [rsp+peinfo.sec_mem_size],eax	;size in memory

lodsd	;RVA
add rdi,rax		;section vbase

lodsd	;size
mov [rsp+peinfo.sec_vbase],rdi
mov [rsp+peinfo.sec_file_size],eax	;size in file

lodsd	;offset
add rsi,0x0C
mov [rsp+peinfo.sec_file_off],eax

lodsd	;attrib
shr eax,16
bt eax,9	;discard
jc near .section_discard

mov [rsp+peinfo.sec_attrib],ax

;ebx -> size in memory
test bx,(PAGE_SIZE-1)
jz .section_aligned

add ebx,PAGE_SIZE

.section_aligned:
shr ebx,12		;page count
mov [rsp+peinfo.sec_page_count],ebx

;rdi -> section vbase
;ebx -> page count
.virtual_alloc:
call PmmAlloc
mov rcx,rdi
mov r8,0x80000000_00000103
mov rdx,rax
call MapPage
add rdi,PAGE_SIZE
dec ebx
jnz .virtual_alloc

mov r8d,[rsp+peinfo.sec_file_size]	;size in file
mov rdi,[rsp+peinfo.sec_vbase]	;section vbase
mov ebx,[rsp+peinfo.sec_mem_size]	;size in memory
test r8d,r8d
mov rcx,rdi
jz .nofile
xor eax,eax
add rdi,r8
sub ebx,r8d
mov edx,[rsp+peinfo.sec_file_off]	;offset
cmovs ebx,eax	;eax == 0
call ReadFile

xor eax,eax
test ebx,ebx
mov ecx,ebx		;remaining size
jz .nofile
shr ecx,3
rep stosq		;zeroing remaining area
and bl,7
movzx ecx,bl
jz .nofile
rep stosb

.nofile:

;mov [rsp+peinfo.sec_size],ebx	;pagecount
mov ax,WORD [rsp+peinfo.sec_attrib]
xor r8,r8

bt ax,13	;X
jc .attrib_X
bts r8,63

.attrib_X:

bt ax,15	;W

jnc .attrib_W
bts r8,1

.attrib_W:

inc r8
bts r8,8

.section_attrib:
mov rcx,[rsp+peinfo.sec_vbase]
mov edx,[rsp+peinfo.sec_page_count]
;r8 -> attrib
call VirtualProtect


.section_discard:

dec WORD [rsp+peinfo.section]
jnz .section


;unmap CMN_BUF
mov rcx,GAP_VBASE
xor rdx,rdx
xor r8,r8
call MapPage


mov rcx,[rsp+peinfo.vbase]
mov edx,[rsp+peinfo.entry]
mov rsp,rbp
add rdx,rcx
sub rsp,0x20


call rdx	;rcx module base

nop

BugCheck:
hlt
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
mov rax,rsi
div DWORD [r12+sysinfo.FAT_cluster]

mov rcx,rdx	;remainder

mov rdx,FAT_KRNL_CLUSTER
mov eax,[rdx+4*rax]

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


PmmAlloc:
mov ecx,[r12+sysinfo.kernel_page]
mov eax,ecx
cmp ecx,0x70
jae .fail
shl eax,12
inc ecx
add eax,0x10000
mov [r12+sysinfo.kernel_page],ecx
ret

.fail:
call BugCheck
int3


;rcx VA
;returns VA of PT
get_PT:
mov rax,HIGHADDR+0x10000
cmp rcx,rax
jb .fail
cmp ecx,0x40000000		;1G
jae .fail

mov edx,ecx		;VA within 1G
mov rax,HIGHADDR+PDT0
shr edx,21	;PDT index
mov rcx,[rax+8*rdx]
bt cx,0
mov rdx,0x7FFFFFFF_FFFFF000
jnc .fail
;get PA of PT
and rcx,rdx
mov rax,HIGHADDR
cmp rcx,0x10000		;direct map
jae .fail
add rax,rcx
ret

.fail:
call BugCheck
int3

;MapAddress VirtualAddr,PhysicalAddr,attrib
;PMM==0 && attrib==0	unmap
MapPage:
push rbx

push rdx	;PA
push r8		;attrib
mov rbx,rcx	;VA

call get_PT	;rcx VA

mov edx,ebx	;VA within 1G

shr edx,12-3
and edx,(PAGE_SIZE-1)
add rdx,rax

pop r8		;attrib
pop rax		;PA
or rax,r8
mov [rdx],rax

invlpg [rbx]

pop rbx
ret


;VA count attrib
VirtualProtect:
push rbx

push rdx	;count
push r8		;attrib
mov rbx,rcx	;VA

.next:
mov rcx,rbx
call get_PT
mov edx,ebx	;VA within 1G

shr edx,12-3
and edx,(PAGE_SIZE-1)
add rdx,rax
mov rcx,[rdx]
mov rax,0x7FFFFFFF_FFFFF000

and rcx,rax
mov rax,[rsp+8]
or rcx,[rsp]	;attrib
dec rax
mov [rdx],rcx
mov [rsp+8],rax

invlpg [rbx]
add rbx,PAGE_SIZE
test rax,rax
jnz .next

add rsp,0x10

pop rbx
ret