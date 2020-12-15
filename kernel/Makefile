export CL_OPTIONS=/nologo /c /GS- /Ox /Z7 /Zc\:rvalueCast,strictStrings /DEBUG\:FULL /fp\:strict /TP /Wall /KERNEL /Fobin\\
INC_PATH=.\\include\\ ..\\util\\include\\ ..\\cpu\\include\\ ..\\..\\util\\include\\

export INC_PATH:=$(addprefix /I ,$(INC_PATH))

export DEFINES:=$(addprefix /D ,$(DEFINES))

MODULES= util sync cpu exception image memory dev

COFUOS: $(MODULES) ../defines
	$(CL_PREFIX) /nologo /GS- /Ox /Zi /DEBUG\:FULL /fp\:strict /TP /Wall /KERNEL /Fobin\\ /Febin\\COFUOS.sys /Fdbin\\COFUOS.pdb /I ..\\util\\include\\ /I util\\include\\ $(DEFINES) $(wildcard *.cpp) /link /FIXED /MACHINE\:X64 /ENTRY\:krnlentry /NODEFAULTLIB /SUBSYSTEM\:NATIVE /STACK\:0x10000,0x2000 /HEAP\:0,0 /BASE\:0xFFFF800002000000 $(wildcard bin/*.obj) $(CL_POSTFIX)
	$(TOOL_PREFIX) ../tools/FAT32_editor.exe ..\\COFUOS.vhd < FAT32_kernel.txt $(TOOL_POSTFIX)
$(MODULES):
	@cd $@ && $(MAKE)

clean:
	-cd bin && $(RM_PREFIX) '*' $(RM_POSTFIX)
	-$(RM_PREFIX) '*.obj' $(RM_POSTFIX)

.PHONY: COFUOS $(MODULES) clean
