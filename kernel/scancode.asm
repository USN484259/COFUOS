[bits 64]

global scancode_table

section .rdata

scancode_table:
incbin "scancode.bin"