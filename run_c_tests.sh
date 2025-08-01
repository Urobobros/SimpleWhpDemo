#!/bin/sh
set -e
# Build and run all C unit tests in the tests directory.
CFLAGS="-DIO_TEST_ACCESS -Itests -ISimpleWhpDemo"
SRCS="SimpleWhpDemo/io.c SimpleWhpDemo/portlog.c SimpleWhpDemo/dma.c SimpleWhpDemo/fdc.c SimpleWhpDemo/pic.c SimpleWhpDemo/pit.c SimpleWhpDemo/serial.c SimpleWhpDemo/keyboard.c SimpleWhpDemo/nmi.c SimpleWhpDemo/timer.c SimpleWhpDemo/stubs.c"
for t in tests/test_*.c; do
  base=$(basename "$t" .c)
  out="/tmp/${base}.exe"
  echo "[BUILD] $t -> $out"
  gcc $CFLAGS $SRCS "$t" -o "$out"
  echo "[RUN]   $out"
  "$out"
  echo
done
