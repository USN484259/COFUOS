
@set PATH=D:\bin\;D:\VC\bin\amd64\;D:\COFUOS\tools\


cl /Oxi /I D:\VC\include\ /I D:\win10_kits\include\ucrt\ /I D:\win10_kits\include\shared\ /I D:\win10_kits\include\um /I D:\COFUOS\util\include\ /I D:\COFUOS\kernel\process\include\ /I D:\udis86-1.7.2\ /MT /TP /W4 /EHsc COFUdbg.cpp disasm.cpp  pipe.cpp  symbol.cpp /link /MACHINE:X64 /LIBPATH:D:\VC\lib\amd64\ /LIBPATH:D:\win10_kits\lib\ucrt\x64\ /LIBPATH:D:\win10_kits\lib\um\x64\ /LIBPATH:D:\udis86-1.7.2\ /SUBSYSTEM:CONSOLE

