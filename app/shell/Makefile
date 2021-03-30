FONT_PATH=$(COFUOS_ROOT)/tools/font_maker/bin/font.o

all:	bin/shell.exe

bin/shell.exe:	*.cpp $(FONT_PATH)
	$(MINGW_CC) $(CPPFLAGS) $(wildcard *.cpp) $(FONT_PATH) -o $@ -e uos_entry -luos -Wl,--relax,--stack=0x4000,--no-seh

.PHONY:	all