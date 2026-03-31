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

/* client de pilotage beuip */
int main(int n, char* p[])
{
int sid, taille;
struct sockaddr_in sock;
char msg_out[LBUF+1];

    /* verification des arguments minimums */
    if (n < 3) {
        fprintf(stderr, "utilisation : %s code donnee [message_prive]\n", p[0]);
        return 1;
    }

    /* creation du socket udp */
    if ((sid = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
        perror("socket");
        return 2;
    }

    /* configuration destination : securite imposee sur 127.0.0.1 */
    bzero(&sock, sizeof(sock));
    sock.sin_family = AF_INET;
    sock.sin_port = htons(PORT);
    sock.sin_addr.s_addr = inet_addr("127.0.0.1");

    /* construction du message selon le code beuip */
    if (p[1][0] == '4') {
        /* cas message prive : insertion du separateur nul */
        if (n < 4) return 3;
        sprintf(msg_out, "4BEUIP%s", p[2]);
        taille = 6 + strlen(p[2]);
        msg_out[taille] = '\0'; /* separateur manuel entre pseudo et texte */
        taille++;
        strcpy(msg_out + taille, p[3]);
        taille += strlen(p[3]);
    } else if (p[1][0] == '5') {
        /* cas message a tous : code 5 + beuip + message */
        sprintf(msg_out, "5BEUIP%s", p[2]);
        taille = strlen(msg_out);
    } else {
        /* cas general pour codes 1, 2, 3 */
        sprintf(msg_out, "%sBEUIP%s", p[1], p[2]);
        taille = strlen(msg_out);
    }

    /* envoi de la commande au serveur local */
    if (sendto(sid, msg_out, taille, 0, (struct sockaddr *)&sock, sizeof(sock)) == -1) {
        perror("sendto");
        return 4;
    }

    printf("commande %s relayee au serveur local\n", p[1]);

    return 0;
}
