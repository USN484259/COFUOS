INC_PATH:=$(COFUOS_ROOT) $(COFUOS_ROOT)/util/include/ $(COFUOS_ROOT)/app/libuos/include
LIB_PATH:=$(COFUOS_ROOT)/app/libuos/bin

export INC_PATH:=$(addprefix -I,$(INC_PATH))
export LIB_PATH:=$(addprefix -L,$(LIB_PATH))
export DEFINES:=$(addprefix -D,$(DEFINES))
export CPPFLAGS=$(CC_OPTIONS) $(INC_PATH) $(LIB_PATH) $(DEFINES)

SRC_LIST = $(wildcard *.cpp)

PROG_LIST = $(patsubst %.cpp,bin/%.exe,$(SRC_LIST))

app:	libuos shell hack $(PROG_LIST)
	guestfish -a $(VHD_PATH) < guestfish_script


libuos:
	@cd $@ && $(MAKE)

shell:	libuos
	@cd $@ && $(MAKE)

hack:
	@cd $@ && $(MAKE)
# bin/%.exe.clean:	bin/%.exe
# 	$(OBJCOPY) -S $< $@

bin/%.exe:	%.cpp libuos
	$(MINGW_CC) $(CPPFLAGS) -e uos_entry $< -o $@ -luos -Wl,--relax,--stack=0x4000,--no-seh
#	$(OBJCOPY) -S $@ $@.clean

# bin/test.exe:	bin/test.o
# 	$(MINGW_LD) -b pei-x86-64 -nostdlib $< -o $@

# bin/test.o:	test.asm
# 	$(NASM) -f win64 $< -o $@

.PHONY:	app libuos shell hack