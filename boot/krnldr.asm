IA32_EFER equ 0xC0000080
IA32_APIC_BASE equ 0x1B

HIGHADDR equ 0xFFFF_8000_0000_0000

SYSINFO_BASE equ 0x0500
SYSINFO_LEN equ 0x100

GDT_BASE equ 0x0600
GDT_LIM equ 0x600	;96 entries

IDT_BASE equ 0x0C00
IDT_LIM equ 0x400	;64 entries

PAGE_SIZE equ 0x1000
SECTOR_SIZE equ 0x200

LDR_STK equ 0x02000

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

%define LIMITED_RESOLUTION 1

;	physical Memory layout

;	000000		001000		RWT	GDT & IDT
;	001000		002000		RWT	PMMSCAN & VBE_scan & fatal ISR & loader stk
;	002000		003000		RX	loader & MP entry & sysinfo
;	003000		004000		RWT	PL4T
;	004000		005000		RWT	PDPT low
;	005000		006000		RWT	PDPT high
;	006000		007000		RWT	PDT
;	007000		008000		RWT	PT0
;	008000		009000		RWT	krnl PT
;	009000		00A000		RWT	mapper PT
;	00A000		010000		RW	avl
;-------------direct map---------------------
;	010000		?			?	kernel pages


struc sysinfo
.sig			resq 1
.ACPI_RSDT		resq 1

.PMM_avl_top	resq 1
.volume_base	resq 1

.kernel_page	resw 1
.port			resw 7

.VBE_addr		resd 1
.VBE_mode		resw 1
.VBE_scanline	resw 1
.VBE_width		resw 1
.VBE_height		resw 1
.VBE_bpp		resw 1
				resw 1

; meaningless for kernel
.exfat_SIG		resd 1
.cluster_size	resd 1
.lba_FAT		resq 1
.lba_HEAP		resq 1


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
db 1111_0010_b
db 1100_1111_b
db 0		;user ds

dw 0xFFFF
dw 0
db 0
db 1111_1010_b
db 1100_1111_b
db 0		;user cs


dd 0
dd 1111_0010_0000_0000_b	;user64 DS

dd 0
dd 0000_0000_0010_0000_1111_1000_0000_0000_b	;user64 CS

gdt64_len equ ($-gdt64_base)

GDT_SYS_CS equ 0x08
GDT_SYS_DS equ 0x10

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

cmp [si-5],dl	;dl == 0
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
cmp [si-0x05],dl	;dx == 0
jnz .next
;get XSDT
mov eax,[si-0x0C]
mov edx,[si-0x08]
mov cl,[si-0x15]	;ACPI version
mov [es:SYSINFO_BASE+sysinfo.ACPI_RSDT],eax
mov [es:SYSINFO_BASE+sysinfo.ACPI_RSDT+4],edx
mov [es:SYSINFO_BASE+sysinfo.ACPI_RSDT+7],cl
stc
ret

.next:
clc
ret


BSP_entry:
mov ss,cx
mov sp,LDR_STK

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

mov si,0x7C40
mov di,SYSINFO_BASE+sysinfo.volume_base
mov cx,4
es rep movsw

mov si,0x0400
mov di,SYSINFO_BASE+sysinfo.port
mov cx,7
es rep movsw

mov WORD [es:SYSINFO_BASE+sysinfo.VBE_bpp],24

mov si,0x7C64
mov di,SYSINFO_BASE+sysinfo.exfat_SIG
mov cx,2
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
mov ax,0x200
jnz abort16

cmp [es:PMMSCAN_BASE+0x204],ax
js abort16

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
jnz NEAR .end

test BYTE [es:PMMSCAN_BASE],0x80	;linear frame buffer
jz .video_loop


mov cx,[es:PMMSCAN_BASE+0x12]	;width
mov dx,[es:PMMSCAN_BASE+0x14]	;height

cmp cx,800
jb .video_loop

cmp dx,600
jb .video_loop

%if LIMITED_RESOLUTION == 0
cmp cx,[es:SYSINFO_BASE+sysinfo.VBE_width]
jb .video_loop
cmp dx,[es:SYSINFO_BASE+sysinfo.VBE_height]
jb .video_loop
%else
xor ax,ax
cmp ax,[es:SYSINFO_BASE+sysinfo.VBE_width]
jz .initial
cmp ax,[es:SYSINFO_BASE+sysinfo.VBE_height]
jz .initial

cmp cx,[es:SYSINFO_BASE+sysinfo.VBE_width]
ja .video_loop
cmp dx,[es:SYSINFO_BASE+sysinfo.VBE_height]
ja .video_loop

.initial:
%endif

movzx ax,BYTE [es:PMMSCAN_BASE+0x19]
cmp ax,[es:SYSINFO_BASE+sysinfo.VBE_bpp]
jb .video_loop

;save current video mode

mov di,SYSINFO_BASE+sysinfo.VBE_mode
mov [es:di],bx	;mode
mov [es:di + 4],cx	;width
mov [es:di + 6],dx	;height
mov [es:di + 8],ax	;bpp

mov cx,[es:PMMSCAN_BASE+0x200+0x04]	;VBE version
mov dx,[es:PMMSCAN_BASE+0x10]
sub cx,0x0300	; >= VBE 3.0
js .nvbe3
mov dx,[es:PMMSCAN_BASE+0x32]
.nvbe3:
mov [es:di + 2],dx	;scanline

mov di,SYSINFO_BASE+sysinfo.VBE_addr
mov ax,[es:PMMSCAN_BASE+0x28]
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
mov esp,LDR_STK
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

; cmp al,7
; jb cpuid0_basic

; xor ecx,ecx
; mov eax,7
; cpuid

; mov [SYSINFO_BASE+sysinfo.cpu],ebx		;SMEP (SMAP INVPCID ?)

; cpuid0_basic:

mov eax,1
cpuid

bt edx,15	;cmovcc
jnc abort

bt edx,13	;PGE
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

; cmp al,8
; jb cpuid8_basic
; mov eax,0x80000008
; cpuid

; mov [SYSINFO_BASE+sysinfo.bus],ax	;MAXPHYADDR

; cpuid8_basic:

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
call map_page_32

mov edi,PL4T+0x800
mov ebx,PDPT8
inc ecx
call map_page_32

mov edi,PDPT0
mov ebx,PDT0
inc ecx
call map_page_32

mov edi,PDPT8
mov ebx,PDT0
inc ecx
call map_page_32

mov edi,PDT0
mov ebx,PT0
inc ecx
call map_page_32

mov edi,PDT0 + 0x08	;PDT[1] -> PT_MAP
mov ebx,PT_MAP
inc ecx
call map_page_32

;direct map boot area
mov edi,PT0
xor ebx,ebx
mov edx,0x80000103
mov cl,2
call map_page_32

mov ebx,0x2000
mov edx,0x101		;RX
inc ecx
call map_page_32

mov ebx,PT_BASE
mov edx,0x8000011B	;CD WT
;mov edx,0x80000103
mov cl,PT_LEN>>12
call map_page_32

; up to 0x10000
mov ebx,PT_BASE+PT_LEN
mov edx,0x80000103
mov cl,0x10-((PT_BASE+PT_LEN)>>12)
call map_page_32


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


map_page_32:	;edi PT		ebx PMM		ecx cnt		edx attrib

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
mov rcx,(GDT_LIM-1) << 48
push rdx
push rcx
lgdt [rsp+6]

mov ax,GDT_SYS_DS
mov ds,ax
mov es,ax
mov fs,ax
mov gs,ax
mov ss,ax
mov rsp,HIGHADDR+LDR_STK

mov ecx,8
mov eax,.cs_reload
push rcx
push rax

call qword [rsp]

.cs_reload:

mov rdx,LM_high
jmp rdx

align 16

CODE64OFF equ ($-$$)

section codehigh vstart=0xFFFF8000_00002000+CODE64OFF

folder_name db 'BOOT',0
image_name db 'COFUOS',0


align 8

; ecx : folder_cluster
; rdx : filename
; return : rax => file_entrance
; using CMN_BUF_VBASE
find_file:
push rbx
push rsi
push rdi
push rdx

mov ebx,[r12 + sysinfo.cluster_size]
lea eax,[ecx - 2]
shr ebx,9
mov r8d,8
mul ebx
mov rdx,[r12 + sysinfo.lba_HEAP]
add rdx,rax
cmp ebx,8
mov rsi,CMN_BUF_VBASE
cmova ebx,r8d

.load:
mov rcx,rsi
call read_sector
add rsi,SECTOR_SIZE
dec ebx
jnz .load

mov rdi,CMN_BUF_VBASE
lea rbx,[rsi - 0x60]

; assume name not exceed 14 characters
.find:
mov al,[rdi]
test al,al
jz .fail
cmp al, 0x85
jnz .next
cmp byte [rdi + 1], 2
jnz .next
cmp byte [rdi + 0x20],0xC0
jnz .next
cmp word [rdi + 0x40],0xC1
jnz .next
movzx rcx, byte [rdi + 0x23]
lea rsi,[rdi + 0x42]
cmp cl,14
mov rdx,[rsp]
ja .next

.match:
lodsw
cmp ah,0
jnz .next
cmp al,[rdx]
jz .equal
cmp al,0x60
jbe .next
cmp al,0x7A
ja .next
xor al,[rdx]
cmp al,0x20
jnz .next
.equal:
inc rdx
test ax,ax
loopnz .match

test rcx,rcx
mov rax,rdi
jnz .next

cmp [rdx],cl
; found
jz .end

.next:
add rdi,0x20
cmp rdi,rbx
jbe .find

.fail:
xor rax,rax

.end:
pop rdx
pop rdi
pop rsi
pop rbx
ret


LM_high:
mov r12,HIGHADDR+SYSINFO_BASE

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
mov rbp,cr4
xor eax,eax
cpuid
cmp al,7
jb .nosmep

xor ecx,ecx
mov eax,7
cpuid
bt ebx,7
jnc .nosmep
bts rbp,20	;SMEP


.nosmep:
bts rbp,7	;PGE
btr rbp,16	;FSGSBASE

mov cr4,rbp
;no need to reload CR3 since setting PGE flushs TLB

exFAT_verify:

mov rbx,[r12 + sysinfo.volume_base]
mov rcx,CMN_BUF_VBASE
mov rdx,rbx
call read_sector
mov rsi,CMN_BUF_VBASE
mov eax,[r12 + sysinfo.exfat_SIG]
cmp word [rsi + 0x1FE],0xAA55
jnz .fail
cmp [rsi + 0x64],eax
mov rdx,'EXFAT   '
jnz .fail
cmp [rsi + 3],rdx
mov ecx,0x35
jnz .fail
lea rdi,[rsi + 0x0B]
xor eax,eax
repz scasb
mov edx,[rsi + 0x58]
jnz .fail
mov eax,SECTOR_SIZE
cmp [rsi + 0x40],rbx
mov cl,[rsi + 0x6D]
jnz .fail
shl eax,cl
cmp word [rsi + 0x68],0x0100
mov [r12 + sysinfo.cluster_size],eax
jnz .fail
cmp byte [rsi + 0x6C],9
mov al,[rsi + 0x6E]
jnz .fail
add rdx,rbx
xor ecx,ecx
dec al
mov [r12 + sysinfo.lba_HEAP],rdx
js .fail
jz .calc_fat

cmp al,1
jnz .fail
bt word [rsi + 0x6A],0
cmovc ecx,[rsi + 0x54]
jmp .calc_fat

.fail:
call BugCheck
int3

.calc_fat:
add ecx,[rsi + 0x50]
mov edi,[rsi + 0x60]
add rcx,rbx
mov [r12 + sysinfo.lba_FAT],rcx


find_krnl:
sub rsp,0x40

call pmm_alloc
mov rdx,rax
mov rcx,GAP_VBASE
mov r8,0x80000000_00000003
mov [rsp+peinfo.headerpmm],rax
call map_page

; edi as root directory
mov ecx,edi
mov rdx,folder_name
call find_file
test rax,rax
mov rsi,rax
jz .fail
bt word [rsi + 4],4
mov rax,[rsi + 0x28]
jnc .fail
cmp rax,[rsi + 0x38]
mov ecx,[rsi + 0x34]
jnz .fail
test rax,rax
jz .fail
test ecx,ecx
mov rdx,image_name
jz .fail
call find_file
test rax,rax
mov rsi,rax
jz .fail

bt word [rsi + 4],4
mov rbx,[rsi + 0x28]
jc .fail
cmp rbx,[rsi + 0x38]
mov eax,[rsi + 0x34]
ja .fail
test rbx,rbx
mov rdi,FAT_KRNL_CLUSTER
jz .fail
test eax,eax
mov r8d,[r12 + sysinfo.cluster_size]
jz .fail
cmp rbx,0x70000	; over 448k, possibly overwrite BIOS
;mov r8d,SECTOR_SIZE
ja .fail
stosd
xor edx,edx
mov eax,ebx
xor ecx,ecx
div r8d

test edx,edx
setnz cl

bt word [rsi + 0x20],9
lea ebx,[eax + ecx - 1]

; build cluster table
; ebx (cluster_count - 1)
; eax cur_cluster
jc .linear_cluster

bts r13,63
mov eax,[rdi - 4]
mov rsi,CMN_BUF_VBASE

.cluster_loop:
mov r14d,eax
shr eax,7
and r14d,0x7F
cmp rax,r13
mov rcx,rsi
jz .same_fat

mov rdx,[r12 + sysinfo.lba_FAT]
add rdx,rax
mov r13,rax
call read_sector

.same_fat:
mov eax,[rsi + 4*r14]
cmp eax,2
jb .fail
cmp eax,0xFFFFFFF7
jz .fail
ja .cluster_done

stosd
dec ebx
jmp .cluster_loop

.cluster_done:
test ebx,ebx
jz load_image

.fail:
call BugCheck
int3

.linear_cluster:
test ebx,ebx
mov eax,[rdi - 4]
jz load_image
mov ecx,ebx
.linear_loop:
inc eax
stosd
loop .linear_loop

load_image:

mov rsi,GAP_VBASE

mov rcx,rsi
xor rdx,rdx
mov rdi,rsi
mov r8,SECTOR_SIZE
call read_file
add rdi,SECTOR_SIZE


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
jnz .fail

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
mov ebx,eax
ja .fail

mov rcx,rdi
mov edx,SECTOR_SIZE
mov r8d,eax
call read_file
add rdi,rbx

.smallheader:

;lodsd
;lodsd	;(DLLCharacteristics<<16) | Subsystem
add rsi,8

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


;ignore heap
;lodsq
;lodsq

;lodsd	;reserved
add rsi,0x14

lodsd	;datadir cnt

shl eax,3
add rsi,rax		;skip datadir

mov rbp,rdi

.makestk:
sub rdi,PAGE_SIZE
call pmm_alloc
mov r8,0x80000000_00000103
mov rdx,rax
mov rcx,rdi
call map_page

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
call map_page


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
call pmm_alloc
mov rcx,rdi
mov r8,0x80000000_00000103
mov rdx,rax
call map_page
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
call read_file

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
call virtual_protect


.section_discard:

dec WORD [rsp+peinfo.section]
jnz .section


;unmap CMN_BUF
mov rcx,GAP_VBASE
xor rdx,rdx
xor r8,r8
call map_page


mov rcx,[rsp+peinfo.vbase]
mov edx,[rsp+peinfo.entry]
mov rsp,rbp
add rdx,rcx
sub rsp,0x20


call rdx	;rcx module base

.fail:

BugCheck:
;hlt
mov dx,0x92
in al,dx
or al,1
out dx,al
hlt
jmp BugCheck


;read_sector dst,LBA
read_sector:

mov r8,rdi

mov r9d,edx		;LBA
mov rdi,rcx		;dst

mov dx,0x1F6
in al,dx
bt ax,4			;drive select
mov eax,r9d
setc cl
shr eax,24
shl cl,4
and al,0x0F		;LBA 27..24
or cl,0xE0

or al,cl
out dx,al

mov dx,0x1F2
mov al,1		;cnt
out dx,al

mov eax,r9d
inc dx
out dx,al		;0x1F3

shr eax,8
inc dx
out dx,al		;0x1F4

shr eax,8
inc dx
out dx,al		;0x1F5

mov al,0x20		;read sector
mov dx,0x1F7
out dx,al

xor ecx,ecx
.wait:

inc ecx
jz .fail

pause
;dx == 0x1F7
in al,dx
test al,0x80
jnz .wait
test al,0010_0001_b
jnz .fail
test al,8
jz .wait


mov dx,0x1F0
mov ecx,256
rep insw

mov rdi,r8
ret

.fail:
call BugCheck
int3


; rcx : dst
; rdx : offset
; r8 : size
read_file:

test dx,0x1FF
jnz .fail

test r8w,0x1FF
jnz .fail

shr rdx,9	;in sector
shr r8,9	;in sector

push rbp
push rsi
push rdi
push rbx

mov ebp,[r12 + sysinfo.cluster_size]
mov rsi,rdx		;offset in sector
mov rdi,rcx		;dst
mov rbx,r8		;size in sector
shr ebp,9

.read:
xor rdx,rdx
mov rax,rsi
div ebp

mov rcx,rdx	;remainder

mov rdx,FAT_KRNL_CLUSTER
mov eax,[rdx+4*rax]

sub eax,2

mul ebp

test edx,edx
jnz .fail
add eax,ecx
mov rdx,[r12 + sysinfo.lba_HEAP]
mov rcx,rdi
add rdx,rax
call read_sector

add rdi,SECTOR_SIZE
inc rsi

dec rbx
jnz .read


pop rbx
pop rdi
pop rsi
pop rbp

ret

.fail:
call BugCheck
int3


pmm_alloc:
mov cx,[r12+sysinfo.kernel_page]
movzx eax,cx
cmp cx,0x70
jae .fail
shl eax,12
inc ecx
add eax,0x10000
mov [r12+sysinfo.kernel_page],cx
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
map_page:
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
virtual_protect:
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