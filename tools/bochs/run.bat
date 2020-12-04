taskkill /f /im bochs.exe

del ..\..\*.lock

bochs.exe -f bochsrc.bxrc

