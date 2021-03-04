export COFUOS_ROOT:=$(CURDIR)

export MINGW_CC=mingw-gcc
export MINGW_LD=mingw-ld
export CC=g++
export NASM=nasm
export OBJCOPY=objcopy
export STRIP=strip
export CP=cp

all: boot font FAT32_editor COFUOS app

boot:
	@cd boot && $(MAKE) $@
	
COFUOS: font FAT32_editor app
	@cd kernel && $(MAKE) $@

font:
	@cd tools/font_maker && $(MAKE) $@

FAT32_editor:
	@cd tools/FAT32_editor && $(MAKE) $@

app:
	@cd app && $(MAKE) $@

clean:
	@cd boot && $(MAKE) clean
	@cd kernel && $(MAKE) clean
	@cd tools/FAT32_editor && $(MAKE) clean
	@cd tools/font_maker && $(MAKE) clean
	@cd app && $(MAKE) clean
help:
	@echo "help text"

.PHONY: all boot font FAT32_editor COFUOS app clean help
