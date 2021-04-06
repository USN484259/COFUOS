all:	bin/echo.exe bin/disk_read.exe bin/ps.exe

bin/%.exe:	%.cpp
	$(MINGW_CC) $(CPPFLAGS) -e uos_entry $< -o $@ -luos -Wl,--relax,--stack=0x4000,--no-seh
	$(OBJCOPY) -S $@ $@.clean

.PHONY:	all