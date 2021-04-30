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

all: boot COFUOS app

boot:	make_boot
	@cd boot && $(MAKE) $@

util:
	@cd util && $(MAKE) $@

COFUOS:	util
	@cd kernel && $(MAKE) $@

font:
	@cd tools/font_maker && $(MAKE) $@

make_boot:
	@cd tools/make_boot && $(MAKE) $@

app:	util font
	@cd app && $(MAKE) $@

clean:
	@cd boot && $(MAKE) clean
	@cd kernel && $(MAKE) clean
	@cd tools/make_boot && $(MAKE) clean
	@cd tools/font_maker && $(MAKE) clean
	@cd app && $(MAKE) clean
help:
	@echo "help text"

.PHONY: all boot util font make_boot COFUOS app clean help
