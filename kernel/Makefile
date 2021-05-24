DEFINES:=$(DEFINES) UOS_KRNL KSP_OFF=0x48 CONX_OFF=0x58
INC_PATH:=$(COFUOS_ROOT)/kernel/ $(COFUOS_ROOT)/util/include/ $(COFUOS_ROOT)/kernel/util/include/ ./include/

export CC_OPTIONS:=$(CC_OPTIONS) -mno-mmx -mno-sse -mno-sse2
export INC_PATH:=$(addprefix -I,$(INC_PATH))
export DEFINES:=$(addprefix -D,$(DEFINES))
export CPPFLAGS=$(CC_OPTIONS) $(INC_PATH) $(DEFINES)

MODULES= util sync exception memory dev process filesystem interface

COFUOS:	bin/COFUOS.sys.clean
	guestfish add $(VHD_PATH) : launch : mount /dev/sda / : mkdir-p /boot/ : upload $< /boot/$@

bin/COFUOS.sys.clean:	bin/COFUOS.sys
	$(OBJCOPY) -S $< $@

bin/COFUOS.sys: $(MODULES) bin/hal.o $(COFUOS_ROOT)/util/include/*.hpp $(MODULES)/include/*.hpp
	$(MINGW_CC) $(CPPFLAGS) $(wildcard *.cpp) $(wildcard $(COFUOS_ROOT)/util/bin/*.o) $(wildcard bin/*.o) -o $@ -e krnlentry -Wl,--relax,--image-base=0xFFFF800002000000,--stack=0x4000,--no-seh

$(MODULES):
	@cd $@ && $(MAKE)

bin/hal.o:	hal.asm
	$(NASM) -f win64 $(DEFINES) $< -o $@

clean:
	-cd bin && $(RM_PREFIX) '*' $(RM_POSTFIX)
	-$(RM_PREFIX) '*.o' $(RM_POSTFIX)

.PHONY: COFUOS $(MODULES) clean
