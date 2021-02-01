all:	bin/waitable.o bin/thread.o bin/process.o bin/core_state.o

bin/%.o:	%.cpp
	$(MINGW_CC) $(CPPFLAGS) -c $< -o $@
	$(CP) $@ ../bin/
%.cpp:	include/*.hpp ../util/include/*.hpp $(COFUOS_ROOT)/util/include/*.hpp

.PHONY: all
