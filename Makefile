default : servudp cliudp servbeuip

cliudp : cliudp.c
	cc -Wall -o cliudp cliudp.c

servudp : servudp.c
	cc -Wall -o servudp servudp.c

# ajout de la règle pour le serveur beuip avec l'option trace par defaut
servbeuip : servbeuip.c
	cc -Wall -DTRACE -o servbeuip servbeuip.c

clean :
	rm -f cliudp servudp servbeuip
