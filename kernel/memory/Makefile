all:	bin/pm.o bin/vm.o bin/kernel_vspace.o bin/user_vspace.o

bin/%.o:	%.cpp
	$(MINGW_CC) $(CPPFLAGS) -c $< -o $@
	$(CP) $@ ../bin/

%.cpp:	include/*.hpp ../util/include/*.hpp $(COFUOS_ROOT)/util/include/*.hpp

.PHONY: all



