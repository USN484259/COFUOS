#export CL_OPTIONS=/nologo /c /ffreestanding /std\:c++14 /GS- /GL /Ox /Z7 /FAsc /DEBUG\:FULL /TP /Wall /KERNEL /Fobin\\ /Fabin\\ 
#/Zc\:rvalueCast,strictStrings

export CC_OPTIONS= -g -std=c++14 -Og -masm=intel -ffreestanding -fno-exceptions -fno-rtti -Wall -Wno-invalid-offsetof -nostdlib -m64 -mno-mmx -mno-sse -mno-sse2

#INC_PATH=.\\include\\ ..\\util\\include\\ ..\\cpu\\include\\ ..\\..\\util\\include\\ ..\\sync\\include\\ 

DEFINES:=$(DEFINES) CONX_OFF=0x48



#INC_PATH= $(addsuffix \\include\\,$(MODULES))
#INC_PATH:=$(addprefix $(COFUOS_ROOT)\\kernel\\,$(INC_PATH))

INC_PATH:=$(COFUOS_ROOT)/kernel/ $(COFUOS_ROOT)/util/include/ $(COFUOS_ROOT)/kernel/util/include/ ./include/

export INC_PATH:=$(addprefix -I,$(INC_PATH))

export DEFINES:=$(addprefix -D,$(DEFINES))

export CPPFLAGS=$(CC_OPTIONS) $(INC_PATH) $(DEFINES)

MODULES= util sync cpu exception image memory dev process

COFUOS:	bin/COFUOS.sys

bin/COFUOS.sys: $(MODULES) bin/scancode.o $(COFUOS_ROOT)/util/include/*.hpp $(MODULES)/include/*.hpp
#	$(CL_PREFIX) /nologo /GS- /GL /Ox /Zi /DEBUG\:FULL /fp\:strict /TP /Wall /KERNEL /Fobin\\ /Febin\\COFUOS.sys /Fdbin\\COFUOS.pdb $(INC_PATH) $(DEFINES) $(wildcard *.cpp) /link /FIXED /LTCG /MACHINE\:X64 /ENTRY\:krnlentry /NODEFAULTLIB /SUBSYSTEM\:NATIVE /STACK\:0x10000,0x2000 /HEAP\:0,0 /BASE\:0xFFFF800002000000 $(wildcard bin/*.obj) $(CL_POSTFIX)
#	$(TOOL_PREFIX) ../tools/FAT32_editor.exe ..\\COFUOS.vhd < FAT32_kernel.txt $(TOOL_POSTFIX)
	$(MINGW_CC) $(CPPFLAGS) $(wildcard *.cpp) $(wildcard bin/*.o) -o bin/COFUOS.sys -e krnlentry -Wl,--relax,--image-base=0xFFFF800002000000,--stack=0x4000,--no-seh
	#,--exclude-all-symbols
	#$(OBJCOPY) --strip-unneeded --heap 0,0 --stack 0x4000,0x1000 bin/COFUOS.sys.tmp bin/COFUOS.sys
	$(OBJCOPY) -S bin/COFUOS.sys bin/COFUOS.sys.clean
	#--strip-debug
	#$(WINE) ../tools/FAT32_editor.exe ..\\COFUOS.vhd < FAT32_kernel.txt
	../tools/FAT32_editor/bin/FAT32_editor ../COFUOS.vhd < FAT32_kernel.txt

$(MODULES):
	@cd $@ && $(MAKE)

bin/scancode.o: scancode.asm scancode.bin
	$(NASM) -f win64 scancode.asm -o bin/scancode.o

clean:
	-cd bin && $(RM_PREFIX) '*' $(RM_POSTFIX)
	-$(RM_PREFIX) '*.o' $(RM_POSTFIX)

.PHONY: COFUOS $(MODULES) clean
