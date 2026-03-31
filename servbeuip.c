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
struct sockaddr_in dest_sock;
socklen_t ls;
char buf[LBUF+1];
char msg_out[LBUF+1];
int opt = 1;
unsigned long sender_ip;
char sender_pseudo[LBUF];
char *pseudo_dest, *message_corps;

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
        if (nb_recv >= 6 && strncmp(buf + 1, "BEUIP", 5) == 0) {
            
            /* cas code 3 : affichage de la table des couples */
            if (buf[0] == '3') {
                printf("\n--- liste des presents (%d) ---\n", user_count);
                for (i = 0; i < user_count; i++) {
                    printf("ip : %s | pseudo : %s\n", addrip(table[i].ip), table[i].pseudo);
                }
                printf("------------------------------\n");
            }
            /* cas code 4 : transfert de message prive */
            else if (buf[0] == '4') {
                pseudo_dest = buf + 6;
                /* recherche du message apres le \0 du pseudo */
                message_corps = pseudo_dest + strlen(pseudo_dest) + 1;
                
                int found = 0;
                for (i = 0; i < user_count; i++) {
                    if (strcmp(table[i].pseudo, pseudo_dest) == 0) {
                        found = 1;
                        bzero(&dest_sock, sizeof(dest_sock));
                        dest_sock.sin_family = AF_INET;
                        dest_sock.sin_port = htons(PORT);
                        dest_sock.sin_addr.s_addr = htonl(table[i].ip);
                        
                        /* formatage du message de type 9 */
                        snprintf(msg_out, LBUF, "9BEUIP%s", message_corps);
                        sendto(sid, msg_out, strlen(msg_out), 0, (struct sockaddr *)&dest_sock, sizeof(dest_sock));
                        break;
                    }
                }
                if (!found) fprintf(stderr, "erreur : pseudo %s non trouve\n", pseudo_dest);
            }
            /* cas code 9 : reception d un message prive */
            else if (buf[0] == '9') {
                sender_ip = ntohl(sock.sin_addr.s_addr);
                int found = 0;
                for (i = 0; i < user_count; i++) {
                    if (table[i].ip == sender_ip) {
                        printf("Message de %s : %s\n", table[i].pseudo, buf + 6);
                        found = 1;
                        break;
                    }
                }
                if (!found) printf("Message de %s (inconnu) : %s\n", addrip(sender_ip), buf + 6);
            }
            /* cas codes 1 ou 2 : gestion des utilisateurs */
            else if (buf[0] == '1' || buf[0] == '2') {
                sender_ip = ntohl(sock.sin_addr.s_addr);
                strcpy(sender_pseudo, buf + 6);
                
                known = 0;
                for (i = 0; i < user_count; i++) {
                    if (table[i].ip == sender_ip && strcmp(table[i].pseudo, sender_pseudo) == 0) {
                        known = 1;
                        break;
                    }
                }
                
                if (!known && user_count < MAX_USERS) {
                    table[user_count].ip = sender_ip;
                    strcpy(table[user_count].pseudo, sender_pseudo);
                    user_count++;
#ifdef TRACE
                    printf("nouveau contact : %s (%s)\n", sender_pseudo, addrip(sender_ip));
#endif
                }

                if (buf[0] == '1') {
                    snprintf(msg_out, LBUF, "2BEUIP%s", p[1]);
                    sendto(sid, msg_out, strlen(msg_out), 0, (struct sockaddr *)&sock, ls);
                }
            }
        }
    }
    return 0;
}
