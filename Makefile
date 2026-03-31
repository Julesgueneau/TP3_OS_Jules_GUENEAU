default : servudp cliudp servbeuip clibeuip

# compilation de la librairie creme
creme.o : creme.c creme.h
	cc -Wall -c creme.c

# mise a jour des cibles pour inclure l objet creme
servbeuip : servbeuip.c creme.o
	cc -Wall -DTRACE -o servbeuip servbeuip.c creme.o

clibeuip : clibeuip.c creme.o
	cc -Wall -o clibeuip clibeuip.c creme.o

cliudp : cliudp.c
	cc -Wall -o cliudp cliudp.c

servudp : servudp.c
	cc -Wall -o servudp servudp.c

clean :
	rm -f *.o cliudp servudp servbeuip clibeuip
