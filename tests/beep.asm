bits 16
org 0x100

%define speaker_port 0x61

segment .text
start:
        mov al, 3
        out speaker_port, al
        mov cx, 0xFFFF
.delay:
        loop .delay
        mov al, 0
        out speaker_port, al
        cli
        hlt
