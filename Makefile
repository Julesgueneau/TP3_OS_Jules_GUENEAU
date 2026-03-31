# variables de compilation
CC = cc
# modifier ici pour activer ou desactiver les traces
CFLAGS = -Wall -DTRACE
LDFLAGS = -lreadline

# cibles principales
all: biceps servbeuip clibeuip servudp cliudp

# regle pour la gestion des commandes
gescom.o : gescom.c gescom.h
	$(CC) $(CFLAGS) -c gescom.c

# programme biceps
biceps: biceps.c creme.o gescom.o
	$(CC) $(CFLAGS) -o biceps biceps.c creme.o gescom.o $(LDFLAGS)

# serveur beuip
servbeuip: servbeuip.c creme.o
	$(CC) $(CFLAGS) -o servbeuip servbeuip.c creme.o

# client beuip
clibeuip: clibeuip.c creme.o
	$(CC) $(CFLAGS) -o clibeuip clibeuip.c creme.o

# bibliotheque creme
creme.o: creme.c creme.h
	$(CC) $(CFLAGS) -c creme.c

# utilitaires udp
servudp: servudp.c
	$(CC) $(CFLAGS) -o servudp servudp.c

cliudp: cliudp.c
	$(CC) $(CFLAGS) -o cliudp cliudp.c

# nettoyage
clean:
	rm -f *.o biceps servbeuip clibeuip servudp cliudp
