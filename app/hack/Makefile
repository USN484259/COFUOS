

export DEFINES:=$(addprefix -D,$(DEFINES))

SRC_LIST = $(wildcard *.asm)

OBJ_LIST = $(patsubst %.asm,bin/%.o,$(SRC_LIST))

PROG_LIST = $(patsubst %.asm,bin/%.exe,$(SRC_LIST))

hack:	bin/run.exe $(PROG_LIST)

bin/run.exe:	run.cpp
	$(MINGW_CC) $(CPPFLAGS) $< -o $@ -e uos_entry -luos -Wl,--relax,--stack=0x4000,--no-seh

bin/%.exe:	bin/%.o
	$(MINGW_LD) --relax --stack=0x4000 --no-seh $< -o $@

bin/%.o:	%.asm
	$(NASM) -f win64 $< -o $@

.PHONY:	hack