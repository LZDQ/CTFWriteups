a.o: a.c shared.h
	gcc -c -o a.o a.c
	
b.i: b.c shared.h
	cpp b.c -o b.i # C Pre-Processor

b.s: b.i
	cc b.i -S -o b.s # Compiler

b.o: b.s
	as b.s -o b.o # Assembler

main.o: main.c shared.h
	gcc -c -o main.o main.c

main: a.o b.o main.o
	gcc -lm -o main a.o b.o main.o # Link to executable (but too many details)
	# see https://stackoverflow.com/questions/14163208/how-to-link-c-object-files-with-ld
