@color 2f
nasm -f bin boot.asm -o bin\boot.bin
@echo --------------------------------------------
@pause > nul
nasm -f bin krnldr.asm -o bin\krnldr.bin
@echo --------------------------------------------
@pause > nul
nasm -f win64 ISR.asm -o bin\ISR.obj
@echo --------------------------------------------
@pause > nul
nasm -f win64 hal.asm -o bin\hal.obj
@echo --------------------------------------------
@pause > nul
@REM g++ -std=c++11 -m64 COFUOS.cpp -mcmodel=kernel -o bin\COFUOS.sys

cd bin\
cl /GS /Od /D "_DEBUG" /fp:strict /Zc:wchar_t,auto,rvalueCast /TP /W4 /KERNEL /Fe:COFUOS.sys ..\COFUOS.cpp ..\exception.cpp ..\pe.cpp ..\heap.cpp ..\lock.cpp ..\dbg.cpp /link /RELEASE /FIXED /MACHINE:X64 /NODEFAULTLIB /ENTRY:krnlentry /SUBSYSTEM:NATIVE /HEAP:0,0 /BASE:0xFFFF800002000000 ISR.obj hal.obj
@echo --------------------------------------------
@pause > nul