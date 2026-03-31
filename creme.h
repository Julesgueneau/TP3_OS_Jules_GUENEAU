#ifndef CREME_H
#define CREME_H

#include <netinet/in.h>

#define PORT_BEUIP 9998
#define LBUF 512

/* structure pour la table des contacts */
struct user_info {
    unsigned long ip;
    char pseudo[LBUF];
};

/* initialisation du socket udp et bind */
int creme_init_socket(int port);

/* activation de l option broadcast */
int creme_set_broadcast(int sid);

/* conversion d une adresse ip en chaine */
char *creme_addr_to_str(unsigned long a);

/* envoi d un message au format beuip */
int creme_send_msg(int sid, struct sockaddr_in *dest, char code, const char *pseudo, const char *payload);

#endif
