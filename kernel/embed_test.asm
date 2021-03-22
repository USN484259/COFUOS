[bits 64]

global file_list

section .rdata

align 0x20
file_list:
; dq test_exe_base
; dd (test_exe_top - test_exe_base)
; db '/test.exe',0
; align 0x20 db 0

dq shell_exe_base
dd (shell_exe_top - shell_exe_base)
db '/shell.exe',0
align 0x20, db 0

dq 0
align 0x20, db 0

; test_exe_base:
; incbin "../app/test/bin/test.exe"
; test_exe_top:

shell_exe_base:
incbin "../app/test/bin/shell.exe"
shell_exe_top: