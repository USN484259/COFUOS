My software engineering teacher said that software testing is vital to software products.
The truth is that without kdb stub what I can do is typing 'P' in bochs when 'jump to address' in IDA and find the source line in notepad++.
Not the debugging,but docking two boring text windows together with X-tremely HUGE IDA window in a 1366*768 screen is impossible.
Meanwhile,it is even worse if the kdb stub has wired bugs since 'break to debugger' doesn't mean the kernel freezes,it's just busy waiting the serial port in the stub core and once a byte arrives,RIP have to go through the boring complex and buggy dispatch logic.
So the solution is a kdb simulator compiled using the cpp file same as the kernel do,and use NamedPipe on Windows to pretend to process with 'debug packets'. 
