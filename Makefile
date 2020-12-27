export _PREFIX=wine

export CL_PREFIX=$(_PREFIX) cl.exe
export CL_POSTFIX= 2>/dev/null

export NASM_PREFIX=$(_PREFIX) nasm.exe
export NASM_POSTFIX=

export TOOL_PREFIX=$(_PREFIX)
export TOOL_POSTFIX=

export CP_PREFIX=cp
export CP_POSTFIX=

export RM_PREFIX=find -type f -name
export RM_POSTFIX=-delete

export WIN_INC_PATH=D\:\\VC\\include\\ D\:\\win10_kits\\include\\ucrt\\ D\:\\win10_kits\\include\\shared\\ D\:\\win10_kits\\include\\um\\ 

export WIN_LIB_PATH=D\:\\VC\\lib\\ D\:\\win10_kits\\lib\\ucrt\\x64\\ D\:\\win10_kits\\lib\\um\\x64\\ 

#export COFUOS_ROOT:=$(CURDIR)

export COFUOS_ROOT:=D:\\COFUOS

all: boot font COFUOS COFUdbg

boot:
	@cd boot && $(MAKE) $@
	
COFUOS: font
	@cd kernel && $(MAKE) $@

COFUdbg:
	@cd tools/COFUdbg && $(MAKE) $@

font:
	@cd tools/font_maker && $(MAKE) $@

clean:
	@cd boot && $(MAKE) clean
	@cd kernel && $(MAKE) clean
	@cd tools/COFUdbg && $(MAKE) clean
	@cd tools/font_maker && $(MAKE) clean

help:
	@echo "help text"

.PHONY: all boot font COFUOS COFUdbg clean help
