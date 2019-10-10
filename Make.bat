@color 2f
nasm -f bin boot.asm -o bin\boot.bin
@echo --------------------------------------------
@pause > nul
nasm -f bin krnldr.asm -o bin\krnldr.bin -l krnldr.lst
@echo --------------------------------------------
@pause > nul
nasm -f win64 hal.asm -o bin\hal.obj -l symbol\hal.lst
@echo --------------------------------------------
@pause > nul
REM @g++ -std=c++11 -m64 COFUOS.cpp -mcmodel=kernel -o bin\COFUOS.sys	

rem /EHa 
cl /GS- /Oxi /Zi /DEBUG:FULL /D "_DEBUG" /fp:strict /Zc:wchar_t,auto,rvalueCast /TP /Wall /KERNEL /FAsc /Fasymbol\ /Fobin\ /Febin\COFUOS.sys /Fdbin\COFUOS.pdb COFUOS.cpp global.cpp pe.cpp heap.cpp lock.cpp apic.cpp exception.cpp mm.cpp ps.cpp mp.cpp bits.cpp /link /FIXED /MACHINE:X64 /ENTRY:krnlentry /NODEFAULTLIB /SUBSYSTEM:NATIVE /STACK:0x100000,0x4000 /HEAP:0,0 /BASE:0xFFFF800002000000 bin\hal.obj
@echo --------------------------------------------
@pause > nul

cd symbol\
dbgen -dCOFUOS.db -m..\bin\COFUOS.sys
@echo --------------------------------------------
@pause > nul

cd ..\

FAT32_editor bochs\COFUOS.vhd < FAT32_editor_script.txt
@echo --------------------------------------------
@echo Press any key to start debugging...
@pause > nul

@color 0F
.\bochsdbg.bat