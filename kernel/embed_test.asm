[bits 64]

global test_file_base
global test_file_size

section .rdata
test_file_base:
incbin "../app/bin/test.exe"

test_file_size dq ($-test_file_base)