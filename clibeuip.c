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
int sid, taille;
struct sockaddr_in sock;
char msg_out[LBUF+1];

    /* verification des arguments : code 4 necessite pseudo + message */
    if (n < 3) {
        fprintf(stderr, "utilisation : %s code pseudo [message]\n", p[0]);
        return 1;
    }

    /* creation du socket */
    if ((sid = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
        perror("socket");
        return 2;
    }

    /* configuration destination */
    bzero(&sock, sizeof(sock));
    sock.sin_family = AF_INET;
    sock.sin_port = htons(PORT);
    sock.sin_addr.s_addr = inet_addr("127.0.0.1");

    /* construction du message selon le code */
    if (p[1][0] == '4') {
        if (n < 4) {
            fprintf(stderr, "erreur : le code 4 exige un message en 3eme argument\n");
            return 3;
        }
        /* entete + pseudo */
        sprintf(msg_out, "4BEUIP%s", p[2]);
        taille = 6 + strlen(p[2]);
        /* ajout du \0 separateur */
        msg_out[taille] = '\0';
        taille++;
        /* ajout du message */
        strcpy(msg_out + taille, p[3]);
        taille += strlen(p[3]);
    } else {
        /* cas general pour codes 1, 2, 3 */
        sprintf(msg_out, "%sBEUIP%s", p[1], p[2]);
        taille = strlen(msg_out);
    }

    /* envoi de la taille exacte (incluant le \0 interne pour le code 4) */
    if (sendto(sid, msg_out, taille, 0, (struct sockaddr *)&sock, sizeof(sock)) == -1) {
        perror("sendto");
        return 4;
    }

    printf("commande %s envoyee\n", p[1]);
    return 0;
}
