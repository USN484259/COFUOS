
CL_OPTIONS=/nologo /Ox /MT /TP /W4 /EHsc /Fobin\\ /Febin\\font_maker.exe

#INCLUDE_PATH=/I D\:\\VC\\include\\ /I D\:\\win10_kits\\include\\ucrt\\ /I D\:\\win10_kits\\include\\shared\\ /I D\:\\win10_kits\\include\\um /I D\:\\COFUOS\\util\\include\\

SOURCE_FILES=$(wildcard *.cpp)

LINK_OPTIONS=/link /MACHINE\:X64 /SUBSYSTEM\:CONSOLE

#LIB_PATH=/LIBPATH\:D\:\\VC\\lib\\amd64\\ /LIBPATH\:D\:\\win10_kits\\lib\\ucrt\\x64\\ /LIBPATH\:D\:\\win10_kits\\lib\\um\\x64\\

WIN_INC_PATH:=$(WIN_INC_PATH) D\:\\COFUOS\\util\\include\\ 

WIN_INC_PATH:=$(addprefix /I ,$(WIN_INC_PATH))

WIN_LIB_PATH:=$(addprefix /LIBPATH\:,$(WIN_LIB_PATH))

FONT_NAME=$(basename $(wildcard *.fnt))

font: bin/font.o

bin/font.o: bin/font.bin font.asm
	$(NASM) -f win64 font.asm -o bin/font.o
	#$(CP) bin/font.o ../../kernel/bin/

bin/font.bin: bin/font_maker
	./bin/font_maker $(FONT_NAME).fnt bin/font.bin

clean:
	-cd bin && $(RM_PREFIX) '*' $(RM_POSTFIX)

bin/font_maker: $(SOURCE_FILES)
	$(CC) -g -I $(COFUOS_ROOT)/util/include $< -o $@
	#$(CL_EXE) $(CL_OPTIONS) $(WIN_INC_PATH) $(SOURCE_FILES) $(LINK_OPTIONS) $(WIN_LIB_PATH)
.PHONY: font clean
