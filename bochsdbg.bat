

del bochs\*.lock


start /D bochs\ bochs.exe -q -f bochsrc.bxrc

pause

COFUOS_dbg.exe -p\\.\pipe\com_1 -dsymbol\COFUOS.db -enotepad++.exe

pause