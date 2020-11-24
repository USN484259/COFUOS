[bits 16]

FAT_INFO equ 0x7C00+0x200-0x10
;	FAT_header	resd 1
;	FAT_table	resd 1
;	FAT_data	resd 1
;	FAT_cluster	resw 1
;	BOOT_MAGIC	dw 0xAA55

LOADER_BASE equ 0x2000


section code vstart=0x7c5A

;segment init
xor ax,ax
mov ss,ax
mov sp,0x7c00
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

mov si,LOADER_BASE+0x1C2
lodsw
cmp al,0x0C		;FAT32 with LBA
jnz die

;PAT => partition

mov di,FAT_INFO

lodsw	;skip 2 bytes
lodsw	;LWORD of PAT
mov dx,ax
stosw
lodsw	;HWORD of PAT
push ax
push dx
stosw

;assume PAT at 0x7C00

mov si,0x0D+0x7c00
lodsb	;sectors per cluster => SpC
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

push bx		;SpC
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
pop bx		;SpC


loadfile:	;dx:ax cluster index	es:di target auto inc bx SpC

cmp ax,0xFFF8
jb .pas
cmp dx,0x0FFF
jb .pas

.break:
push LOADER_BASE
ret

.pas:
xor cx,cx
cmp ax,cx
jnz .pass
cmp dx,cx
jz .break

.pass:
mov bp,sp

push dx
push ax

mov cl,128
xor dx,dx

div cx
push dx		;remainder

xor cx,cx
add ax,[FAT_INFO+4]
mov dx,[FAT_INFO+6]
adc dx,cx

call readsector

mov cl,2
pop si
shl si,cl

add si,di

lodsw
mov dx,[si]

push dx
push ax

mov ax,[bp-4]

mul bx

push ax
push dx

mov ax,[bp-2]
mul bx

test dx,dx
jnz die

pop dx
add dx,ax
pop ax
jc die

add ax,[FAT_INFO+8]
adc dx,[FAT_INFO+0x0A]

mov cx,bx
shl cx,1
sub ax,cx
sbb dx,0

call readsector

mov ax,bx
mov cl,9
shl ax,cl
add di,ax

pop ax
pop dx

mov sp,bp
jmp loadfile

