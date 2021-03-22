INC_PATH:=$(COFUOS_ROOT) $(COFUOS_ROOT)/util/ $(COFUOS_ROOT)/util/include/ $(COFUOS_ROOT)/app/libuos/include
LIB_PATH:=$(COFUOS_ROOT)/app/libuos/bin

export CC_OPTIONS= -g -std=c++14 -Og -masm=intel -ffreestanding -fno-exceptions -fno-rtti -Wall -nostdlib -m64
export INC_PATH:=$(addprefix -I,$(INC_PATH))
export LIB_PATH:=$(addprefix -L,$(LIB_PATH))
export DEFINES:=$(addprefix -D,$(DEFINES))
export CPPFLAGS=$(CC_OPTIONS) $(INC_PATH) $(LIB_PATH) $(DEFINES)

app:	libuos test

libuos:
	@cd $@ && $(MAKE)

test:	libuos
	@cd $@ && $(MAKE)

# bin/test.exe:	bin/test.o
# 	$(MINGW_LD) -b pei-x86-64 -nostdlib $< -o $@

# bin/test.o:	test.asm
# 	$(NASM) -f win64 $< -o $@

.PHONY:	app libuos test