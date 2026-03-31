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
#define MAX_USERS 255

/* structure pour stocker les informations du reseau */
struct user_info {
    unsigned long ip;
    char pseudo[LBUF];
};

struct user_info table[MAX_USERS];
int user_count = 0;

char * addrip(unsigned long a)
{
static char b[16];
    sprintf(b,"%u.%u.%u.%u",(unsigned int)(a>>24&0xFF),(unsigned int)(a>>16&0xFF),
           (unsigned int)(a>>8&0xFF),(unsigned int)(a&0xFF));
    return b;
}

int main(int n, char* p[])
{
int sid, nb_recv, i, known;
struct sockaddr_in sock_conf;
struct sockaddr_in sock;
struct sockaddr_in bcast_addr;
socklen_t ls;
char buf[LBUF+1];
char msg_out[LBUF+1];
int opt = 1;
unsigned long sender_ip;
char sender_pseudo[LBUF];

    /* verification de l argument */
    if (n != 2) {
        fprintf(stderr, "utilisation : %s pseudo\n", p[0]);
        return 1;
    }

    /* creation du socket */
    if ((sid = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
        perror("socket");
        return 2;
    }

    /* initialisation de sock_conf pour le bind */
    bzero(&sock_conf, sizeof(sock_conf));
    sock_conf.sin_family = AF_INET;
    sock_conf.sin_port = htons(PORT);
    sock_conf.sin_addr.s_addr = htonl(INADDR_ANY);
    
    if (bind(sid, (struct sockaddr *) &sock_conf, sizeof(sock_conf)) == -1) {
        perror("bind");
        return 3;
    }

    /* activation de l option broadcast sur le socket */
    if (setsockopt(sid, SOL_SOCKET, SO_BROADCAST, &opt, sizeof(opt)) < 0) {
        perror("setsockopt");
        return 4;
    }

    /* preparation et envoi du message broadcast */
    bzero(&bcast_addr, sizeof(bcast_addr));
    bcast_addr.sin_family = AF_INET;
    bcast_addr.sin_port = htons(PORT);
    bcast_addr.sin_addr.s_addr = inet_addr("192.168.88.255");
    
    snprintf(msg_out, LBUF, "1BEUIP%s", p[1]);
    if (sendto(sid, msg_out, strlen(msg_out), 0, (struct sockaddr *)&bcast_addr, sizeof(bcast_addr)) == -1) {
        perror("sendto bcast");
    }

#ifdef TRACE
    printf("serveur demarre sur le port %d avec le pseudo : %s\n", PORT, p[1]);
#endif

    /* boucle principale de reception */
    for (;;) {
        ls = sizeof(sock);
        if ((nb_recv = recvfrom(sid, (void*)buf, LBUF, 0, (struct sockaddr *)&sock, &ls)) == -1) {
            perror("recvfrom");
            continue;
        }
        buf[nb_recv] = '\0';
        
        /* verification de l en-tete du protocole */
        if (nb_recv >= 7 && (buf[0] == '1' || buf[0] == '2') && strncmp(buf + 1, "BEUIP", 5) == 0) {
            sender_ip = ntohl(sock.sin_addr.s_addr);
            strcpy(sender_pseudo, buf + 6);
            
            /* verification des doublons dans la table */
            known = 0;
            for (i = 0; i < user_count; i++) {
                if (table[i].ip == sender_ip && strcmp(table[i].pseudo, sender_pseudo) == 0) {
                    known = 1;
                    break;
                }
            }
            
            /* enregistrement si nouvel utilisateur */
            if (!known && user_count < MAX_USERS) {
                table[user_count].ip = sender_ip;
                strcpy(table[user_count].pseudo, sender_pseudo);
                user_count++;
#ifdef TRACE
                printf("nouveau contact : %s (%s)\n", sender_pseudo, addrip(sender_ip));
#endif
            }

            /* reponse par accuse de reception si broadcast recu */
            if (buf[0] == '1') {
                snprintf(msg_out, LBUF, "2BEUIP%s", p[1]);
                if (sendto(sid, msg_out, strlen(msg_out), 0, (struct sockaddr *)&sock, ls) == -1) {
                    perror("sendto ack");
                }
            }
        }
    }
    return 0;
}
