DEFINES:=$(DEFINES) KSP_OFF=0x48 CONX_OFF=0x58
INC_PATH:=$(COFUOS_ROOT)/kernel/ $(COFUOS_ROOT)/util/include/ $(COFUOS_ROOT)/kernel/util/include/ ./include/

export CC_OPTIONS= -g -std=c++14 -Og -masm=intel -ffreestanding -fno-exceptions -fno-rtti -Wall -Wno-invalid-offsetof -nostdlib -m64 -mno-mmx -mno-sse -mno-sse2
export INC_PATH:=$(addprefix -I,$(INC_PATH))
export DEFINES:=$(addprefix -D,$(DEFINES))
export CPPFLAGS=$(CC_OPTIONS) $(INC_PATH) $(DEFINES)

MODULES= util sync exception memory dev process filesystem interface

COFUOS:	bin/COFUOS.sys

bin/COFUOS.sys: $(MODULES) bin/scancode.o bin/hal.o bin/embed_test.o $(COFUOS_ROOT)/util/include/*.hpp $(MODULES)/include/*.hpp
	$(MINGW_CC) $(CPPFLAGS) $(wildcard *.cpp) $(wildcard bin/*.o) -o bin/COFUOS.sys -e krnlentry -Wl,--relax,--image-base=0xFFFF800002000000,--stack=0x4000,--no-seh
	$(OBJCOPY) -S bin/COFUOS.sys bin/COFUOS.sys.clean
	../tools/FAT32_editor/bin/FAT32_editor ../COFUOS.vhd < FAT32_kernel.txt

$(MODULES):
	@cd $@ && $(MAKE)

bin/scancode.o: scancode.asm scancode.bin
	$(NASM) -f win64 $< -o $@

bin/embed_test.o: embed_test.asm ../app/bin/test.exe
	$(NASM) -f win64 $< -o $@

bin/hal.o:	hal.asm
	$(NASM) -f win64 $(DEFINES) $< -o $@

clean:
	-cd bin && $(RM_PREFIX) '*' $(RM_POSTFIX)
	-$(RM_PREFIX) '*.o' $(RM_POSTFIX)

.PHONY: COFUOS $(MODULES) clean
