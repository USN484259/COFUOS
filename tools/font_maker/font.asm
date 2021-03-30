[bits 64]

global font_base
global font_size
global font_info

section .rdata

align 0x10
font_base:
incbin "bin/font.bin",0x10
font_end:

align 0x10
font_info:
incbin "bin/font.bin",0,0x10

font_size dd (font_end - font_base)
