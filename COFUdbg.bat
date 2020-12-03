taskkill /f /im bochs.exe

del *.lock


start /d .\tools\bochs\ .\tools\bochs\bochs.exe -q -f bochsrc.bxrc

pause

rem tools\COFUOS_dbg.exe -p\\.\pipe\com_1 -dkernel\COFUOS.db

tools\COFUdbg.exe \\.\pipe\com_1 kernel\bin\COFUOS.sys

pause
