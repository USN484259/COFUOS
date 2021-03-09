[bits 64]


SRV_PRINT equ 0
SRV_SELF equ 0x0100
SRV_SLEEP equ 0x0108
SRV_EXIT equ 0x0118
SRV_KILL equ 0x0120
SRV_OPENPROC equ 0x210
SRV_TRY equ 0x0220

section .rdata
str_captured:
db 'captured',0

section .text
; main entry_point, img_base, command, stk_top
main:
mov rsi,r8	;command
xor rdi,rdi
.find_arg:
mov al,[r8]
test al,al
jz .violation
inc r8
cmp al,' '
jnz .find_arg
mov bl,[r8]

.echo:
mov rax,SRV_PRINT
mov rdx,rsi
syscall

cmp bl,'p'
mov ecx,0x01000000
jz .spin

cmp bl,'l'
jz .sleep

cmp bl,'y'
jz .try

cmp bl,'f'
jz .self

cmp bl,'9'
ja .violation
sub bl,'0'
js .violation
jmp .kill

.self:
mov eax,SRV_SELF
syscall
mov ebx,eax
jmp .on_kill

.kill:
mov eax,SRV_SLEEP
mov edx,(10*1000*1000)
syscall

.on_kill:
movzx edx,bl
mov eax,SRV_OPENPROC
syscall
mov edx,eax
mov eax,SRV_KILL
syscall
jmp .end

.on_exception:
mov eax,SRV_PRINT
mov rdx,str_captured
syscall
mov rdi,0x1000_0000
.sleep:
mov eax,SRV_SLEEP
mov edx,(1000*1000)
syscall
jmp .echo

.spin:
pause
dec ecx
js .echo
jmp .spin

.end:
mov eax,SRV_EXIT
syscall

.try:
mov rdx,rdi
mov eax,SRV_TRY
add rdx,.on_exception
syscall

.violation:
mov [rsp+0x80],rax