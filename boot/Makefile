boot: bin/boot.bin bin/krnldr.bin
	../tools/FAT32_editor/bin/FAT32_editor ../COFUOS.vhd < FAT32_boot.txt

clean:
	@echo boot clean no op

bin/boot.bin: boot.asm
	$(NASM) -f bin boot.asm -o bin/boot.bin

bin/krnldr.bin: krnldr.asm
	$(NASM) -f bin krnldr.asm -o bin/krnldr.bin -l bin/krnldr.lst
	
.PHONY: boot clean
