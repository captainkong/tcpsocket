#! /bin/bash
rm -f cjson.o
rm -f libcjson.a
gcc -c cJSON.c -o cjson.o
ar rcs -o libcjson.a cjson.o
rm -f cjson.o