[bits 16]

FAT_INFO equ 0x7C00+0x200-0x10
;	FAT_header	resd 1
;	FAT_table	resd 1
;	FAT_data	resd 1
;	FAT_cluster	resw 1
;	BOOT_MAGIC	dw 0xAA55

LOADER_BASE equ 0x2000


section code vstart=0x7C5A

;segment init
xor ax,ax
mov ss,ax
mov sp,0x7C00
mov ds,ax
mov es,ax
cld



;check LBA availibility
mov ax,0x4100
mov dl,0x80
mov bx,0x55AA
int 0x13

jc die

cmp bx,0xAA55

jnz die

test cl,1

jnz lba_pass

die:
;halt press ctrl alt del to reboot
mov ax,0x0300
mov si,str_fail
xor bx,bx
int 0x10
die_loop:
sti
hlt
jmp die_loop


lba_pass:
xor dx,dx
mov bx,dx
mov ax,dx
inc bx
mov di,LOADER_BASE

call readsector	;read MBR

mov si,LOADER_BASE+0x1BE
find_partition:
lodsw
cmp al,0x80		;active flag
lodsw
jz partition_info
add si,0x0C
cmp si,LOADER_BASE+0x1FE
jae die
jmp find_partition

partition_info:
lodsw
cmp al,0x0C		;FAT32 with LBA
jnz die

mov di,FAT_INFO

lodsw	;skip 2 bytes
lodsw	;LWORD of partition
mov dx,ax
stosw
lodsw	;HWORD of partition
push ax
push dx
stosw

;assume FAT32 head sector at 0x7C00

mov si,0x0D+0x7C00
lodsb	;cluster size
movzx bx,al
lodsw	;reserved sectors
;mov cx,ax
;mov di,sp
pop cx
add ax,cx
stosw
mov dx,ax
xor cx,cx
pop ax
adc ax,cx
stosw
;add [di],ax
;adc [di+2],dx	;FAT table

push ax
push dx

mov si,0x24+0x7c00
lodsw
mov dx,ax
lodsw
xchg dx,ax	;dx:ax  sectors per FAT

add ax,ax
adc dx,dx	;mul by 2

pop cx
add ax,cx
stosw
mov dx,ax
xor cx,cx
pop ax
adc ax,cx
stosw

;add ax,[di]
;adc dx,[di+2]	;dx:ax  FAT DATA

mov [di],bx

mov di,LOADER_BASE

;mov dx,1
;xor di,di
xchg ax,dx

call readsector	;read root directory

push bx		;cluster size
mov si,di
jmp _findfile


;read bx sctors starting from LBA dx:ax to es:di
readsector:
xor cx,cx
push cx
push cx
push dx
push ax
push es
push di
mov cl,0x10
push bx
push cx

mov ax,0x4200
mov dl,0x80
mov si,sp

int 0x13

jc die

add sp,0x10
ret


;kernel file name

strkrnl db 'KRNLDR',0x20,0x20,'BIN'


;	directory entrance
;	+00		BYTE[8] name
;	+08		BYTE[3]	ext
;	+0B		BYTE attrib
;	+14		WORD HWORD of cluster
;	+1A		WORD LWORD of cluster


_nfound:
lea si,[bx+0x20]	;next file

_findfile:
xor cx,cx
cmp cl,BYTE [di]
jz die		;end of directory

mov bx,si
mov di,strkrnl
mov cl,11

_cmp:
lodsb
xor al,byte [di]
test al,0xDF	;not 0x20	case insensitive
jnz _nfound
inc di
loop _cmp
;repne cmpsb


_found:
;lea si,[bx+0x0B]
lodsb
test al,0x10
jnz die		;directory

add si,8	;	+14
;lea si,[bx+0x14]


lodsw
mov dx,ax
;add si,4	;	+1A
lodsw
lodsw


lodsw	;dx:ax first cluster of file

;xchg ax,dx
mov di,LOADER_BASE
pop bx		;cluster size


loadfile:	;dx:ax cluster index	es:di target auto inc bx cluster size

cmp ax,0xFFF8
jb .pas
cmp dx,0x0FFF
jb .pas

push LOADER_BASE
ret

.pas:
test dx,dx
jnz .pass
cmp ax,2
jb die

.pass:
mov bp,sp

push dx
push ax

mov cl,128
;xor dx,dx

div cx
push dx		;remainder

xor cx,cx
add ax,[FAT_INFO+4]	;FAT_lo
mov dx,[FAT_INFO+6]	;FAT_hi
adc dx,cx

call readsector

mov cl,2
pop si		;remainder
shl si,cl	;offset in sector

add si,di

lodsw
mov dx,[si]

;dx:ax	next cluster
push dx
push ax

mov ax,[bp-4]	;this_cluster_lo

mul bx

push ax
push dx

mov ax,[bp-2]	;this_cluster_hi
mul bx

test dx,dx
jnz die

pop dx
add dx,ax
pop ax
jc die

add ax,[FAT_INFO+8]
adc dx,[FAT_INFO+0x0A]

xor cx,cx
sub ax,2
sbb dx,cx

call readsector

mov ax,bx
mov cl,9
shl ax,cl
add di,ax

pop ax
pop dx

mov sp,bp
jmp loadfile

str_fail db 'Boot failed',0