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

all: boot COFUOS COFUdbg

boot:
	@cd boot && $(MAKE) $@
	
COFUOS:
	@cd kernel && $(MAKE) $@

COFUdbg:
	@cd tools/COFUdbg && $(MAKE) $@

clean:
	@cd boot && $(MAKE) clean
	@cd kernel && $(MAKE) clean
	@cd tools/COFUdbg && $(MAKE) clean

help:
	@echo "help text"

.PHONY: all boot COFUOS COFUdbg clean help
