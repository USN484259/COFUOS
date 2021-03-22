all:	bin/spin_lock.o bin/rwlock.o bin/semaphore.o bin/event.o bin/pipe.o

bin/%.o:	%.cpp
	$(MINGW_CC) $(CPPFLAGS) -c $< -o $@
	$(CP) $@ ../bin/

%.cpp:	include/*.hpp ../util/include/*.hpp $(COFUOS_ROOT)/util/include/*.hpp

.PHONY: all

