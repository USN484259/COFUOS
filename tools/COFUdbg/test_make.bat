
@set PATH=D:\bin\;D:\VC\bin\amd64\;D:\COFUOS\tools\


cl /Oxi /I D:\VC\include\ /I D:\win10_kits\include\ucrt\ /I D:\win10_kits\include\shared\ /I D:\win10_kits\include\um /MT /TP /W4 /EHsc COFUdbg_test.cpp /link /MACHINE:X64 /LIBPATH:D:\VC\lib\amd64\ /LIBPATH:D:\win10_kits\lib\ucrt\x64\ /LIBPATH:D:\win10_kits\lib\um\x64\ /SUBSYSTEM:CONSOLE

