all:	bin/kdb.o bin/exception.o

bin/%.o:	%.cpp
	$(MINGW_CC) $(CPPFLAGS) -c $< -o $@
	$(CP) $@ ../bin/

%.cpp:	include/*.hpp ../util/include/*.hpp $(COFUOS_ROOT)/util/include/*.hpp

.PHONY: all
