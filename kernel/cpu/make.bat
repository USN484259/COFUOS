
@set PATH=D:\bin\;D:\VC\bin\amd64\;D:\COFUOS\tools\

nasm -f win64 hal.asm -o bin\hal.obj
@if %ERRORLEVEL% neq 0 goto fail

cl /nologo /c /GS- /Ox /Z7 /I .\include\ /I ..\util\include\ /I ..\..\util\include\ /DEBUG:FULL /D "_DEBUG" /fp:strict /TP /Wall /KERNEL /Fobin\ IF_guard.cpp
@if %ERRORLEVEL% neq 0 goto fail

exit 0

:fail
exit 1
