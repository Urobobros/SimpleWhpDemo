bits 16
org 0x100

segment .text
start:
        mov si,msg
.next:
        lodsb
        test al,al
        jz .halt
        mov ah,0x0E
        int 0x10
        jmp .next
.halt:
        cli
        hlt

segment .data
msg:
        db "Hello from CGA",10,0
