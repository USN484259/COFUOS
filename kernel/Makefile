export CL_OPTIONS=/nologo /c /std\:c++14 /GS- /GL /Ox /Z7 /FAsc /DEBUG\:FULL /fp\:strict /TP /Wall /KERNEL /Fobin\\ /Fabin\\ 
#/Zc\:rvalueCast,strictStrings

#INC_PATH=.\\include\\ ..\\util\\include\\ ..\\cpu\\include\\ ..\\..\\util\\include\\ ..\\sync\\include\\ 



MODULES= util sync cpu exception image memory dev

#INC_PATH= $(addsuffix \\include\\,$(MODULES))
#INC_PATH:=$(addprefix $(COFUOS_ROOT)\\kernel\\,$(INC_PATH))

INC_PATH:=$(COFUOS_ROOT)\\kernel\\ $(COFUOS_ROOT)\\util\\include\\ $(COFUOS_ROOT)\\kernel\\util\\include\\ .\\include\\

export INC_PATH:=$(addprefix /I ,$(INC_PATH))

export DEFINES:=$(addprefix /D ,$(DEFINES))

COFUOS: $(MODULES) ../defines bin/scancode.obj
	$(CL_PREFIX) /nologo /GS- /GL /Ox /Zi /DEBUG\:FULL /fp\:strict /TP /Wall /KERNEL /Fobin\\ /Febin\\COFUOS.sys /Fdbin\\COFUOS.pdb $(INC_PATH) $(DEFINES) $(wildcard *.cpp) /link /FIXED /LTCG /MACHINE\:X64 /ENTRY\:krnlentry /NODEFAULTLIB /SUBSYSTEM\:NATIVE /STACK\:0x10000,0x2000 /HEAP\:0,0 /BASE\:0xFFFF800002000000 $(wildcard bin/*.obj) $(CL_POSTFIX)
	$(TOOL_PREFIX) ../tools/FAT32_editor.exe ..\\COFUOS.vhd < FAT32_kernel.txt $(TOOL_POSTFIX)
$(MODULES):
	@cd $@ && $(MAKE)

bin/scancode.obj: scancode.asm scancode.bin
	$(NASM_PREFIX) -f win64 scancode.asm -o bin\\scancode.obj $(NASM_POSTFIX)

clean:
	-cd bin && $(RM_PREFIX) '*' $(RM_POSTFIX)
	-$(RM_PREFIX) '*.obj' $(RM_POSTFIX)

.PHONY: COFUOS $(MODULES) clean
