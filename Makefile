default : servudp cliudp servbeuip clibeuip

# règle pour le client udp initial
cliudp : cliudp.c
	cc -Wall -o cliudp cliudp.c

# règle pour le serveur udp initial
servudp : servudp.c
	cc -Wall -o servudp servudp.c

# règle pour le serveur beuip avec traces actives
servbeuip : servbeuip.c
	cc -Wall -DTRACE -o servbeuip servbeuip.c

# règle pour le nouveau client de test beuip
clibeuip : clibeuip.c
	cc -Wall -o clibeuip clibeuip.c

# nettoyage des executables
clean :
	rm -f cliudp servudp servbeuip clibeuip
