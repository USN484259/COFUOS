taskkill /f /im bochs.exe

del ..\..\*.lock


start bochs.exe -q -f bochsrc.bxrc

pause

cd ..\..

tools\COFUdbg.exe \\.\pipe\com_1 kernel\bin\COFUOS.sys

