#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#define PORT 9998
#define LBUF 512

int main(int n, char* p[])
{
int sid;
struct sockaddr_in sock;
char msg_out[LBUF+1];

    /* verification des arguments */
    if (n != 3) {
        fprintf(stderr, "utilisation : %s code pseudo\n", p[0]);
        fprintf(stderr, "codes : 1=broadcast, 2=ack\n");
        return 1;
    }

    /* creation du socket */
    if ((sid = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
        perror("socket");
        return 2;
    }

    /* configuration de l adresse de destination (local) */
    bzero(&sock, sizeof(sock));
    sock.sin_family = AF_INET;
    sock.sin_port = htons(PORT);
    sock.sin_addr.s_addr = inet_addr("127.0.0.1");

    /* formatage du message : code + BEUIP + pseudo */
    snprintf(msg_out, LBUF, "%sBEUIP%s", p[1], p[2]);

    /* envoi au serveur local */
    if (sendto(sid, msg_out, strlen(msg_out), 0, (struct sockaddr *)&sock, sizeof(sock)) == -1) {
        perror("sendto");
        return 3;
    }

    printf("commande %s envoyee au serveur local\n", p[1]);

    return 0;
}
