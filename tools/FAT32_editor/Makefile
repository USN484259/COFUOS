HEADER_FILES=$(wildcard *.hpp)

FAT32_editor: bin/FAT32_editor


bin/FAT32_editor: bin/FAT32.o bin/MBR.o
	$(CC) -g -I $(COFUOS_ROOT)/util/include FAT32_editor.cpp bin/FAT32.o bin/MBR.o -o $@
	#$(CL_EXE) $(CL_OPTIONS) $(WIN_INC_PATH) $(SOURCE_FILES) $(LINK_OPTIONS) $(WIN_LIB_PATH)

bin/FAT32.o:	FAT32.cpp $(HEADER_FILES)
	$(CC) -c -g -I $(COFUOS_ROOT)/util/include $< -o $@

bin/MBR.o:	MBR.cpp $(HEADER_FILES)
	$(CC) -c -g -I $(COFUOS_ROOT)/util/include $< -o $@

clean:
	-cd bin && $(RM_PREFIX) '*' $(RM_POSTFIX)

.PHONY: FAT32_editor clean