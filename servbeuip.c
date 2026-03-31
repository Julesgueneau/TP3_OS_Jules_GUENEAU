#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <signal.h>
#include <unistd.h>

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
int sid_global;
char pseudo_global[LBUF];

char * addrip(unsigned long a)
{
static char b[16];
    sprintf(b,"%u.%u.%u.%u",(unsigned int)(a>>24&0xFF),(unsigned int)(a>>16&0xFF),
           (unsigned int)(a>>8&0xFF),(unsigned int)(a&0xFF));
    return b;
}

/* routine d interruption pour le depart */
void handler_stop(int sig) {
    struct sockaddr_in bcast_addr;
    char msg[LBUF];
    int opt = 1;

    bzero(&bcast_addr, sizeof(bcast_addr));
    bcast_addr.sin_family = AF_INET;
    bcast_addr.sin_port = htons(PORT);
    bcast_addr.sin_addr.s_addr = inet_addr("192.168.88.255");

    setsockopt(sid_global, SOL_SOCKET, SO_BROADCAST, &opt, sizeof(opt));
    /* notification code 0 */
    snprintf(msg, LBUF, "0BEUIP%s", pseudo_global);
    sendto(sid_global, msg, strlen(msg), 0, (struct sockaddr *)&bcast_addr, sizeof(bcast_addr));
    
    printf("\narret du serveur\n");
    close(sid_global);
    exit(0);
}

int main(int n, char* p[])
{
int nb_recv, i, j, known;
struct sockaddr_in sock_conf, sock, bcast_addr, dest_sock;
socklen_t ls;
char buf[LBUF+1], msg_out[LBUF+1];
int opt = 1;
unsigned long sender_ip;
char sender_pseudo[LBUF];

    /* verification argument */
    if (n != 2) {
        fprintf(stderr, "usage : %s pseudo\n", p[0]);
        return 1;
    }

    strcpy(pseudo_global, p[1]);
    /* capture du signal ctrl+c */
    signal(SIGINT, handler_stop);

    /* creation socket */
    if ((sid_global = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
        perror("socket");
        return 2;
    }

    /* configuration bind */
    bzero(&sock_conf, sizeof(sock_conf));
    sock_conf.sin_family = AF_INET;
    sock_conf.sin_port = htons(PORT);
    sock_conf.sin_addr.s_addr = htonl(INADDR_ANY);
    
    if (bind(sid_global, (struct sockaddr *) &sock_conf, sizeof(sock_conf)) == -1) {
        perror("bind");
        return 3;
    }

    /* mode broadcast */
    if (setsockopt(sid_global, SOL_SOCKET, SO_BROADCAST, &opt, sizeof(opt)) < 0) {
        perror("setsockopt");
        return 4;
    }

    /* annonce initiale */
    bzero(&bcast_addr, sizeof(bcast_addr));
    bcast_addr.sin_family = AF_INET;
    bcast_addr.sin_port = htons(PORT);
    bcast_addr.sin_addr.s_addr = inet_addr("192.168.88.255");
    
    snprintf(msg_out, LBUF, "1BEUIP%s", p[1]);
    sendto(sid_global, msg_out, strlen(msg_out), 0, (struct sockaddr *)&bcast_addr, sizeof(bcast_addr));

#ifdef TRACE
    printf("serveur %s en ligne\n", p[1]);
#endif

    for (;;) {
        ls = sizeof(sock);
        if ((nb_recv = recvfrom(sid_global, (void*)buf, LBUF, 0, (struct sockaddr *)&sock, &ls)) == -1) {
            perror("recvfrom");
            continue;
        }
        buf[nb_recv] = '\0';
        
        if (nb_recv >= 6 && strncmp(buf + 1, "BEUIP", 5) == 0) {
            
            /* filtrage securite local */
            if ((buf[0] == '3' || buf[0] == '4' || buf[0] == '5') && 
                sock.sin_addr.s_addr != inet_addr("127.0.0.1")) {
                continue;
            }

            /* code 0 : suppression d un utilisateur */
            if (buf[0] == '0') {
                sender_ip = ntohl(sock.sin_addr.s_addr);
                for (i = 0; i < user_count; i++) {
                    if (table[i].ip == sender_ip) {
#ifdef TRACE
                        printf("depart de : %s\n", table[i].pseudo);
#endif
                        for (j = i; j < user_count - 1; j++) table[j] = table[j+1];
                        user_count--;
                        break;
                    }
                }
            }
            /* code 3 : liste des presents */
            else if (buf[0] == '3') {
                printf("--- table des presents (%d) ---\n", user_count);
                for (i = 0; i < user_count; i++) printf("%s : %s\n", table[i].pseudo, addrip(table[i].ip));
            }
            /* code 4 : envoi message prive */
            else if (buf[0] == '4') {
                char *dst = buf + 6;
                char *txt = dst + strlen(dst) + 1;
                for (i = 0; i < user_count; i++) {
                    if (strcmp(table[i].pseudo, dst) == 0) {
                        bzero(&dest_sock, sizeof(dest_sock));
                        dest_sock.sin_family = AF_INET;
                        dest_sock.sin_port = htons(PORT);
                        dest_sock.sin_addr.s_addr = htonl(table[i].ip);
                        snprintf(msg_out, LBUF, "9BEUIP%s", txt);
                        sendto(sid_global, msg_out, strlen(msg_out), 0, (struct sockaddr *)&dest_sock, sizeof(dest_sock));
                    }
                }
            }
            /* code 5 : message a tous */
            else if (buf[0] == '5') {
                for (i = 0; i < user_count; i++) {
                    if (strcmp(table[i].pseudo, pseudo_global) == 0) continue;
                    bzero(&dest_sock, sizeof(dest_sock));
                    dest_sock.sin_family = AF_INET;
                    dest_sock.sin_port = htons(PORT);
                    dest_sock.sin_addr.s_addr = htonl(table[i].ip);
                    snprintf(msg_out, LBUF, "9BEUIP%s", buf + 6);
                    sendto(sid_global, msg_out, strlen(msg_out), 0, (struct sockaddr *)&dest_sock, sizeof(dest_sock));
                }
            }
            /* code 9 : reception message prive */
            else if (buf[0] == '9') {
                sender_ip = ntohl(sock.sin_addr.s_addr);
                known = 0;
                for (i = 0; i < user_count; i++) {
                    if (table[i].ip == sender_ip) {
                        printf("message de %s : %s\n", table[i].pseudo, buf + 6);
                        known = 1; break;
                    }
                }
                if (!known) printf("message de %s (inconnu) : %s\n", addrip(sender_ip), buf + 6);
            }
            /* codes 1 ou 2 : gestion des annonces */
            else if (buf[0] == '1' || buf[0] == '2') {
                sender_ip = ntohl(sock.sin_addr.s_addr);
                strcpy(sender_pseudo, buf + 6);
                known = 0;
                for (i = 0; i < user_count; i++) {
                    if (table[i].ip == sender_ip && strcmp(table[i].pseudo, sender_pseudo) == 0) {
                        known = 1; break;
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
                    sendto(sid_global, msg_out, strlen(msg_out), 0, (struct sockaddr *)&sock, ls);
                }
            }
        }
    }
    return 0;
}
