all:	bin/lang.o bin/sysinfo.o	bin/pe64.o

bin/%.o:	%.cpp
	$(MINGW_CC) $(CPPFLAGS) -c $< -o $@
	$(CP) $@ ../bin/

fasthash.c:	include/fasthash.h
%.cpp:	include/*.hpp $(COFUOS_ROOT)/util/include/*.hpp

.PHONY: all

