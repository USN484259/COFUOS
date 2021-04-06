all:	bin/cpu.o bin/apic.o bin/acpi.o bin/pci.o bin/ide.o bin/disk_cache.o bin/timer.o bin/rtc.o bin/display.o bin/ps_2.o

bin/%.o:	%.cpp
	$(MINGW_CC) $(CPPFLAGS) -c $< -o $@
	$(CP) $@ ../bin/

%.cpp:	include/*.hpp ../util/include/*.hpp $(COFUOS_ROOT)/util/include/*.hpp

.PHONY: all
