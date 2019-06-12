@color 2f
nasm -f bin boot.asm -o bin\boot.bin
@echo --------------------------------------------
@pause > nul
nasm -f bin krnldr.asm -o bin\krnldr.bin
@echo --------------------------------------------
@pause > nul
nasm -f win64 hal.asm -o bin\hal.obj
@echo --------------------------------------------
@pause > nul
@REM g++ -std=c++11 -m64 COFUOS.cpp -mcmodel=kernel -o bin\COFUOS.sys	

cl /GS- /Oxi /Zi /DEBUG:FULL /D "_DEBUG" /fp:strict /Zc:wchar_t,auto,rvalueCast /TP /W4 /KERNEL /FAsc /Fabin\ /Fobin\ /Fe:bin\COFUOS.sys /Fdbin\COFUOS.pdb COFUOS.cpp global.cpp pe.cpp heap.cpp lock.cpp apic.cpp exception.cpp mm.cpp ps.cpp mp.cpp /link /FIXED /MACHINE:X64 /ENTRY:krnlentry /NODEFAULTLIB /SUBSYSTEM:NATIVE /HEAP:0,0 /BASE:0xFFFF800002000000 bin\hal.obj
@echo --------------------------------------------
@pause > nul