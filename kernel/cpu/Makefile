all:	bin/apic.o bin/hal.o

bin/hal.o:	hal.asm
	$(NASM) -f win64 $(DEFINES) $< -o $@
	$(CP) $@ ../bin/

bin/%.o:	%.cpp
	$(MINGW_CC) $(CPPFLAGS) -c $< -o $@
	$(CP) $@ ../bin/

%.cpp:	include/*.hpp ../util/include/*.hpp $(COFUOS_ROOT)/util/include/*.hpp

.PHONY: all


