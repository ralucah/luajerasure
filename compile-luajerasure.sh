#!/bin/sh

# clean
rm luajerasure.o luajerasure > /dev/null 2>&1

# obtain object file
gcc  -I/usr/include/lua5.2 -mmmx -msse -DINTEL_SSE -msse2 -DINTEL_SSE2 -msse3 -DINTEL_SSE3 -mssse3 -msse4.1 -DINTEL_SSE4 -msse4.2 -DINTEL_SSE4 -mavx -fPIC -c -I../include luajerasure.c -o luajerasure.o
gcc  -fPIC -c -I../include data_and_coding_printer.c -o data_and_coding_printer.o

# obtain executable file
gcc -shared -llua5.2 -mmmx -msse -DINTEL_SSE -msse2 -DINTEL_SSE2 -msse3 -DINTEL_SSE3 -mssse3 -msse4.1 -DINTEL_SSE4 -msse4.2 -DINTEL_SSE4 -mavx -fPIC -o luajerasure.so ./luajerasure.o ./data_and_coding_printer.o ../src/.libs/libJerasure.a ../../gf-complete/src/.libs/libgf_complete.a 
