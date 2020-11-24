_text segment

public bitswap64

bitswap64:
mov rax,rcx
bswap rax
ret


_text ends


end