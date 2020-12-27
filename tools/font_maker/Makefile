
CL_OPTIONS=/nologo /Ox /MT /TP /W4 /EHsc /Fobin\\ /Febin\\font_maker.exe

#INCLUDE_PATH=/I D\:\\VC\\include\\ /I D\:\\win10_kits\\include\\ucrt\\ /I D\:\\win10_kits\\include\\shared\\ /I D\:\\win10_kits\\include\\um /I D\:\\COFUOS\\util\\include\\

SOURCE_FILES=$(wildcard *.cpp)

LINK_OPTIONS=/link /MACHINE\:X64 /SUBSYSTEM\:CONSOLE

#LIB_PATH=/LIBPATH\:D\:\\VC\\lib\\amd64\\ /LIBPATH\:D\:\\win10_kits\\lib\\ucrt\\x64\\ /LIBPATH\:D\:\\win10_kits\\lib\\um\\x64\\

WIN_INC_PATH:=$(WIN_INC_PATH) D\:\\COFUOS\\util\\include\\ 

WIN_INC_PATH:=$(addprefix /I ,$(WIN_INC_PATH))

WIN_LIB_PATH:=$(addprefix /LIBPATH\:,$(WIN_LIB_PATH))

FONT_NAME=$(basename $(wildcard *.fnt))

font: bin/font.obj
	$(CP_PREFIX) bin/font.obj ../../kernel/bin/ $(CP_POSTFIX)

bin/font.obj: bin/font.bin font.asm
	$(NASM_PREFIX) -f win64 font.asm -o bin\\font.obj $(NASM_POSTFIX)

bin/font.bin: bin/font_maker.exe
	$(TOOL_PREFIX) ./bin/font_maker.exe $(FONT_NAME).fnt bin/font.bin $(TOOL_POSTFIX)

clean:
	-cd bin && $(RM_PREFIX) '*' $(RM_POSTFIX)

bin/font_maker.exe: $(SOURCE_FILES)
	$(CL_PREFIX) $(CL_OPTIONS) $(WIN_INC_PATH) $(SOURCE_FILES) $(LINK_OPTIONS) $(WIN_LIB_PATH) $(CL_POSTFIX)
.PHONY: font clean
