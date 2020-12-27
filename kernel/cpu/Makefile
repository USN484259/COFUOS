all:
	$(CL_PREFIX) $(CL_OPTIONS) $(INC_PATH) $(DEFINES) apic.cpp $(CL_POSTFIX)
	$(NASM_PREFIX) -f win64 hal.asm -o bin\\hal.obj $(NASM_POSTFIX)
	$(CP_PREFIX) bin/*.obj ../bin/ $(CP_POSTFIX)

