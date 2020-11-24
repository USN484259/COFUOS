@color 2f
set PATH=D:\bin\;D:\COFUOS\tools\

nasm -f bin boot.asm -o bin\boot.bin
@if %ERRORLEVEL% neq 0 goto fail
@echo --------------------------------------------

nasm -f bin krnldr.asm -o bin\krnldr.bin -l bin\krnldr.lst
@if %ERRORLEVEL% neq 0 goto fail
@echo --------------------------------------------

FAT32_editor ..\COFUOS.vhd < FAT32_boot.txt
@if %ERRORLEVEL% neq 0 goto fail
@echo --------------------------------------------

@goto end

@:fail
@color 6f
@echo Failed
@echo --------------------------------------------

@:end
@pause
@color

