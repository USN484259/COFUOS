
app:	bin/test.exe

bin/test.exe:	bin/test.o
	$(MINGW_LD) -b pei-x86-64 -nostdlib $< -o $@

bin/test.o:	test.asm
	$(NASM) -f win64 $< -o $@

.PHONY:	app