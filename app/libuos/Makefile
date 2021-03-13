all:	bin/libuos.a

bin/libuos.a:	bin/syscall.o bin/libuos.o bin/uoscrt.o
	$(MINGW_AR) -rcsv $@ $(wildcard bin/*.o)

bin/syscall.o:	syscall.asm
	$(NASM) -f win64 $< -o $@

bin/%.o:	%.cpp
	$(MINGW_CC) $(CPPFLAGS) -c $< -o $@

%.cpp:	include/*.hpp $(COFUOS_ROOT)/util/include/*.hpp

.PHONY: all



