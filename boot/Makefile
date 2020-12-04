boot: bin/boot.bin bin/krnldr.bin

clean:
	@echo boot clean no op

bin/boot.bin: boot.asm
	$(NASM_PREFIX) -f bin boot.asm -o bin\\boot.bin $(NASM_POSTFIX)
	@echo TODO write to FAT32 boot sector

bin/krnldr.bin: krnldr.asm
	$(NASM_PREFIX) -f bin krnldr.asm -o bin\\krnldr.bin $(NASM_POSTFIX)
	$(TOOL_PREFIX) ../tools/FAT32_editor.exe ..\\COFUOS.vhd < FAT32_boot.txt $(TOOL_POSTFIX)
	
.PHONY: boot clean
