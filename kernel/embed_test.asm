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
db 'shell',0
align 0x20, db 0

dq echo_exe_base
dd (echo_exe_top - echo_exe_base)
db 'echo',0
align 0x20, db 0

dq disk_read_exe_base
dd (disk_read_exe_top - disk_read_exe_base)
db 'disk_read',0
align 0x20, db 0

dq ps_exe_base
dd (ps_exe_top - ps_exe_base)
db 'ps',0
align 0x20, db 0

dq 0
align 0x20, db 0

; test_exe_base:
; incbin "../app/test/bin/test.exe"
; test_exe_top:

shell_exe_base:
incbin "../app/shell/bin/shell.exe.clean"
shell_exe_top:

echo_exe_base:
incbin "../app/test/bin/echo.exe.clean"
echo_exe_top:

disk_read_exe_base:
incbin "../app/test/bin/disk_read.exe.clean"
disk_read_exe_top:

ps_exe_base:
incbin "../app/test/bin/ps.exe.clean"
ps_exe_top: