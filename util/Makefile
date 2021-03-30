util:	bin/mem.o bin/fasthash.o

CPPFLAGS:=$(CC_OPTIONS) -mno-mmx -mno-sse -mno-sse2 -Iinclude/

bin/mem.o:	mem.asm
	$(NASM) -f win64 $< -o $@

bin/fasthash.o:	fasthash.c
	$(MINGW_CC) $(CPPFLAGS) -c $< -o $@

.PHONY:	util