boot: bin/boot.bin bin/krnldr.bin

clean:
	@echo boot clean no op

bin/boot.bin: boot.asm
	$(NASM) -f bin boot.asm -o bin/boot.bin
	@echo TODO write to FAT32 boot sector

bin/krnldr.bin: krnldr.asm
	$(NASM) -f bin krnldr.asm -o bin/krnldr.bin -l bin/krnldr.lst
	#$(WINE) ../tools/FAT32_editor.exe ..\\COFUOS.vhd < FAT32_boot.txt
	../tools/FAT32_editor/bin/FAT32_editor ../COFUOS.vhd < FAT32_boot.txt
	
.PHONY: boot clean
