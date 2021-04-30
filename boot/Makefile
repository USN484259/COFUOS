boot: bin/boot.bin bin/krnldr.bin
	../tools/make_boot/bin/make_boot ../COFUOS.vhd bin/boot.bin bin/krnldr.bin

clean:
	@echo boot clean no op

bin/boot.bin: boot.asm
	$(NASM) -f bin boot.asm -o bin/boot.bin

bin/krnldr.bin: krnldr.asm
	$(NASM) -f bin krnldr.asm -o bin/krnldr.bin -l bin/krnldr.lst
	
.PHONY: boot clean
