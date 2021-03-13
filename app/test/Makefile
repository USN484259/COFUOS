all:	bin/test.exe

bin/%.exe:	%.cpp
	$(MINGW_CC) $(CPPFLAGS) -e uos_entry -luos $< -o $@ 

.PHONY:	all