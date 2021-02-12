set debug remote 1
set remotetimeout 10
set tcp connect-timeout 120
set disassembly-flavor intel
set output-radix 16
#display /i $pc
target remote :2590
file bin/COFUOS.sys

