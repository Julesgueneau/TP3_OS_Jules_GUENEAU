CC = cc
# Ajout de -Werror comme exige par le prof
CFLAGS = -Wall -Werror -DTRACE -pthread
LDFLAGS = -lreadline -pthread

all: biceps

gescom.o : gescom.c gescom.h
	$(CC) $(CFLAGS) -c gescom.c

creme.o: creme.c creme.h
	$(CC) $(CFLAGS) -c creme.c

biceps: biceps.c creme.o gescom.o
	$(CC) $(CFLAGS) -o biceps biceps.c creme.o gescom.o $(LDFLAGS)

# Cible specifique pour Valgrind
memory-leak:
	$(CC) -Wall -Werror -DTRACE -pthread -g -O0 -c gescom.c
	$(CC) -Wall -Werror -DTRACE -pthread -g -O0 -c creme.c
	$(CC) -Wall -Werror -DTRACE -pthread -g -O0 -c biceps.c
	$(CC) -Wall -Werror -DTRACE -pthread -g -O0 -o biceps-memory-leaks biceps.o creme.o gescom.o $(LDFLAGS)

clean:
	rm -f *.o biceps biceps-memory-leaks
