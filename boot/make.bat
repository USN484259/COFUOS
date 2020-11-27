set PATH=D:\bin\;D:\COFUOS\tools\

nasm -f bin boot.asm -o bin\boot.bin
@if %ERRORLEVEL% neq 0 goto fail

nasm -f bin krnldr.asm -o bin\krnldr.bin
@if %ERRORLEVEL% neq 0 goto fail

FAT32_editor ..\COFUOS.vhd < FAT32_boot.txt
@if %ERRORLEVEL% neq 0 goto fail

exit 0

:fail
exit 1

