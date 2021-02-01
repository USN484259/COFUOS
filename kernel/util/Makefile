all:	bin/fasthash.o bin/lang.o bin/sysinfo.o	#bin/cui.o

bin/fasthash.o:	fasthash.c
	$(MINGW_CC) $(CPPFLAGS) -c $< -o $@
	$(CP) $@ ../bin/

bin/%.o:	%.cpp
	$(MINGW_CC) $(CPPFLAGS) -c $< -o $@
	$(CP) $@ ../bin/

fasthash.c:	include/fasthash.h
%.cpp:	include/*.hpp $(COFUOS_ROOT)/util/include/*.hpp

.PHONY: all

