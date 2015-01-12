all: radio-rx.c
	gcc -Wall -g -lftdi radio-rx.c -o radio-rx

clean: 
	rm radio-rx 
