[bits 16]

LOADER_BASE equ 0x2000

PACKET_BASE equ 0x1F00

section code vstart=0x7C78

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
mov ax,0x10
mov di,PACKET_BASE
stosw
mov ax,1
stosw
mov ax,LOADER_BASE
stosw
xor ax,ax
stosw
mov si,0x7C40
mov cx,4
rep movsw

xor di,di
mov si,PACKET_BASE

load_slice:
xor cx,cx
add word [si + 8],1
adc [si + 0x0A],cx
adc [si + 0x0C],cx
adc [si + 0x0E],cx

mov ax,0x4200
mov dl,0x80
int 0x13
jc die
mov bx,[si + 4]
cmp ah,0
jnz die
cmp word [bx + 0x1FC],0
jnz die
cmp word [bx + 0x1FE],0xAA55
jnz die

inc di
add word [si + 4],0x1FC
cmp di,8
jb load_slice

push LOADER_BASE
ret

str_fail db 'Boot failed',0
