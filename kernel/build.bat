set PATH=D:\bin\;D:\VC\bin\amd64\;D:\COFUOS\tools\

REM @g++ -std=c++11 -m64 COFUOS.cpp -mcmodel=kernel -o bin\COFUOS.sys

rem /EHa 
cl /nologo /GS- /Ox /Zi /I ..\util\include\ /I util\include\ /DEBUG:FULL /D "_DEBUG" /fp:strict /TP /Wall /KERNEL /Fobin\ /Febin\COFUOS.sys /Fdbin\COFUOS.pdb COFUOS.cpp global.cpp /link /FIXED /MACHINE:X64 /ENTRY:krnlentry /NODEFAULTLIB /SUBSYSTEM:NATIVE /STACK:0x10000,0x2000 /HEAP:0,0 /BASE:0xFFFF800002000000 %*
@if %ERRORLEVEL% neq 0 goto fail


FAT32_editor ..\COFUOS.vhd < FAT32_kernel.txt
@if %ERRORLEVEL% neq 0 goto fail

exit 0

:fail
exit 1

