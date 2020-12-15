
CL_OPTIONS=/nologo /Ox /MT /TP /W4 /EHsc /Fobin\\ /Febin\\COFUdbg.exe

INCLUDE_PATH=/I D\:\\VC\\include\\ /I D\:\\win10_kits\\include\\ucrt\\ /I D\:\\win10_kits\\include\\shared\\ /I D\:\\win10_kits\\include\\um /I D\:\\COFUOS\\util\\include\\ /I D\:\\COFUOS\\kernel\\exception\\include\\ /I D\:\\udis86-1.7.2\\ 

SOURCE_FILES=$(wildcard *.cpp)

LINK_OPTIONS=/link /MACHINE\:X64 /SUBSYSTEM\:CONSOLE

LIB_PATH=/LIBPATH\:D\:\\VC\\lib\\amd64\\ /LIBPATH\:D\:\\win10_kits\\lib\\ucrt\\x64\\ /LIBPATH\:D\:\\win10_kits\\lib\\um\\x64\\ /LIBPATH\:D\:\\udis86-1.7.2\\

COFUdbg: bin/COFUdbg.exe

clean:
	-cd bin && $(RM_PREFIX) '*' $(RM_POSTFIX)

bin/COFUdbg.exe: $(SOURCE_FILES) *.h
	$(CL_PREFIX) $(CL_OPTIONS) $(INCLUDE_PATH) $(SOURCE_FILES) $(LINK_OPTIONS) $(LIB_PATH) $(CL_POSTFIX)
	$(CP_PREFIX) bin/COFUdbg.exe ../ $(CP_POSTFIX)
.PHONY: COFUdbg clean
