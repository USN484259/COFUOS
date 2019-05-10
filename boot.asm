[bits 16]


section code vstart=0x7c5A

;segment init
xor ax,ax
mov ss,ax
mov sp,0x7c00
mov ds,ax
mov ax,0x1000
mov es,ax	;es:di -> 0x10000
mov bp,sp



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
push dx
push dx


mov si,sp
mov di,dx
inc dx
call readsector	;read MBR
;add esp,4
pop ax
pop ax
cld

mov si,0x1C2
es lodsw
cmp al,0x0C		;FAT32 with LBA
jnz die

;PAT => partition

es lodsw	;skip 2 bytes
es lodsw	;LWORD of PAT
mov dx,ax
es lodsw	;HWORD of PAT
push ax
push dx

;assume PAT at 0x7C00

xor ax,ax
mov si,0x0D+0x7c00
lodsb	;sectors per cluster => SpC
mov bx,ax
lodsw	;reserved sectors
;mov cx,ax
mov di,sp
xor dx,dx
add [di],ax
adc WORD [di+2],dx	;FAT table


mov si,0x24+0x7c00
lodsw
mov dx,ax
lodsw
xchg dx,ax	;dx:ax  sectors per FAT

add ax,ax
adc dx,dx	;mul by 2

add ax,[di]
adc dx,[di+2]	;dx:ax  FAT DATA

push dx
push ax




;mov dx,1
;xor di,di
xor dx,dx
mov di,dx
inc dx

mov si,sp
call readsector	;read root directory

push bx		;SpC
;xor di,di
jmp _findfile



;read (dx) sectors starting from (LBA index at ds:si) to (es:di)
readsector:
push bp

push bx

mov bp,sp

xor cx,cx
push cx
push cx
lodsw
mov bx,ax
lodsw
push ax
push bx

push es
push di


push dx

mov cl,0x10
push cx


;	+0	BYTE size
;	+1	BYTE zero
;	+2	WORD sector number
;	+4	WORD offset
;	+6	WORD segment
;	+8	QWORD LBA


mov ax,0x4200
mov dl,0x80
mov si,sp

int 0x13	;LBA read file

jc die	;read fail



mov sp,bp
;pop es
pop bx
pop bp
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
lea di,[bx+0x20]	;next file

_findfile:
xor cx,cx
mov cl,[es:di]
cmp cl,ch
jz die		;end of directory

mov bx,di
mov si,strkrnl
mov cl,11

_cmp:
lodsb
xor al,byte [es:di]
test al,0xDF	;not 0x20	case insensitive
jnz _nfound
inc di
loop _cmp
;repne cmpsb


_found:
lea si,[bx+0x0B]
;mov si,bx
;add si,0x0B
es lodsb
test al,0x10
jnz _nfound		;sub directory

;add si,8	;	+14
lea si,[bx+0x14]


es lodsw
mov dx,ax
;add si,4	;	+1A
es lodsw
es lodsw


es lodsw	;dx:ax first cluster of file

xor di,di	;es:di -->	0x10000
pop bx		;SpC
;mov si,sp	;FAT table
call loadfile

xor ax,ax
push es
push ax
retf	;jmp to 1000:0000




loadfile:	;dx:ax index of FAT   es:di target		bx	sectors per cluster
push bp
push si
push bx

mov bp,sp

;	bp+0	bx
;	bp+2	si
;	bp+4	old bp
;	bp+6	retaddr
;	bp+8	DATA
;	bp+C	FAT

loadfile_loop:



cmp ax,0xFFF8
jb _pas

cmp dx,0x0FFF
jb _pas

_break:
pop bx
pop si
pop bp
ret

_pas:
xor cx,cx
cmp ax,cx
jnz _pass
cmp dx,cx
jnz _pass
jmp _break

_pass:		;valid cluster index

push dx
push ax

;	bp-4	cur cluster index

mov cl,128	;512bytes / DWORD per index
div cx
push dx		;remainder
xor cx,cx
push cx
push ax
;stack top sector offset from FAT

lea si,[bp+0x0C]	;FAT

mov bx,sp

lodsw
add [bx],ax
lodsw
adc [bx+2],ax
;stack top target FAT sector

mov si,sp
xor dx,dx
inc dx
;mov dx,1
call readsector
;add sp,4
pop ax
pop ax

pop bx	;remainder
mov cl,2
shl bx,cl
;bx offset in this sector

mov ax,[es:di+bx]
mov dx,[es:di+bx+2]
push dx
push ax		;dx:ax  next cluster

mov ax,[bp-4]	;LBYTE of cur
mov bx,[bp]		;SpC
mul bx
push dx
push ax

mov ax,[bp-2]	;HBYTE of cur
mul bx
xor cx,cx
cmp dx,cx
jnz die		;DWORD overflow

mov bx,sp
add [bx+2],ax



lea si,[bp+0x08]	;DATA

lodsw
add [bx],ax
lodsw
adc [bx+2],ax

mov ax,[bp]
add ax,ax		;2*SpC
sub WORD [bx],ax
sbb WORD [bx+2],0

;stack top  sector of cur cluster

mov si,bx
mov dx,[bp]		;SpC
call readsector
;add sp,4
pop ax
pop ax

mov dx,[bp]		;SpC
mov cl,9
shl dx,cl
add di,dx		;next block

pop ax
pop dx		;next cluster

mov sp,bp
jmp loadfile_loop





