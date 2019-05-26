@color 2f
nasm -f bin boot.asm -o boot.bin
@echo --------------------------------------------
@pause > nul
nasm -f bin krnldr.asm -o krnldr.bin
@echo --------------------------------------------
@pause > nul
nasm -f win64 ISR.asm -o ISR.obj
@echo --------------------------------------------
@pause > nul
nasm -f win64 hal.asm -o hal.obj
@echo --------------------------------------------
@pause > nul
@REM g++ -std=c++11 -m64 COFUOS.cpp -mcmodel=kernel -o COFUOS.sys
cl /GS /Od /D "_DEBUG" /fp:strict /Zc:wchar_t,auto,rvalueCast /TP /W4 /KERNEL /Fe:COFUOS.sys COFUOS.cpp exception.cpp heap.cpp lock.cpp dbg.cpp /link /RELEASE /FIXED /MACHINE:X64 /NODEFAULTLIB /ENTRY:krnlentry /SUBSYSTEM:NATIVE /HEAP:0,0 /BASE:0xFFFF800002000000 ISR.obj hal.obj
@echo --------------------------------------------
@pause > nul