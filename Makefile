export COFUOS_ROOT:=$(CURDIR)

export MINGW_CC=mingw-gcc
export MINGW_LD=mingw-ld
export MINGW_AR=mingw-ar
export CC=g++
export NASM=nasm
export OBJCOPY=objcopy
export STRIP=strip
export CP=cp
export CC_OPTIONS= -g -std=c++14 -Og -masm=intel -ffreestanding -fno-exceptions -fno-rtti -Wall -Wno-invalid-offsetof -nostdlib -m64

all: boot util font FAT32_editor COFUOS app

boot:
	@cd boot && $(MAKE) $@

util:
	@cd util && $(MAKE) $@

COFUOS: FAT32_editor app util
	@cd kernel && $(MAKE) $@

font:
	@cd tools/font_maker && $(MAKE) $@

FAT32_editor:
	@cd tools/FAT32_editor && $(MAKE) $@

app:	util font
	@cd app && $(MAKE) $@

clean:
	@cd boot && $(MAKE) clean
	@cd kernel && $(MAKE) clean
	@cd tools/FAT32_editor && $(MAKE) clean
	@cd tools/font_maker && $(MAKE) clean
	@cd app && $(MAKE) clean
help:
	@echo "help text"

.PHONY: all boot util font FAT32_editor COFUOS app clean help
