bits 16
org 0x100

%define str_prt_port    0
%define kbd_in_port     1

segment .text
start:
        in al,kbd_in_port
        out str_prt_port,al
        mov al,10
        out str_prt_port,al
        cli
        hlt
