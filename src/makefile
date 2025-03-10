all : fonctions.o main.o main run clear

fonctions.o : fonctions.c fonctions.h structures.h
	gcc -c fonctions.c

main.o : main.c fonctions.o structures.h
	gcc -c main.c

main : main.o fonctions.o structures.h
	gcc -o main main.o fonctions.o structures.h
	
run :
	./main
	
clear :
	rm -f *.o
