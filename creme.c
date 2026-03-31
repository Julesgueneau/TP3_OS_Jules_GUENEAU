#include "creme.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

/* creation et attachement du socket */
int creme_init_socket(int port) {
    int sid;
    struct sockaddr_in sock_conf;
    if ((sid = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) return -1;
    memset(&sock_conf, 0, sizeof(sock_conf));
    sock_conf.sin_family = AF_INET;
    sock_conf.sin_port = htons(port);
    sock_conf.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(sid, (struct sockaddr *)&sock_conf, sizeof(sock_conf)) < 0) return -1;
    return sid;
}

/* reglage de l option de diffusion */
int creme_set_broadcast(int sid) {
    int opt = 1;
    return setsockopt(sid, SOL_SOCKET, SO_BROADCAST, &opt, sizeof(opt));
}

/* utilitaire de conversion ip */
char *creme_addr_to_str(unsigned long a) {
    static char b[16];
    sprintf(b, "%u.%u.%u.%u", (unsigned int)(a >> 24 & 0xFF), (unsigned int)(a >> 16 & 0xFF),
            (unsigned int)(a >> 8 & 0xFF), (unsigned int)(a & 0xFF));
    return b;
}

/* formatage et envoi du datagramme selon le code */
int creme_send_msg(int sid, struct sockaddr_in *dest, char code, const char *pseudo, const char *payload) {
    char buf[LBUF];
    int taille;
    if (code == '4') {
        /* cas message prive avec separateur nul */
        sprintf(buf, "4BEUIP%s", pseudo);
        taille = 6 + strlen(pseudo);
        buf[taille] = '\0';
        taille++;
        strcpy(buf + taille, payload);
        taille += strlen(payload);
    } else {
        /* cas general codes 0, 1, 2, 3, 5, 9 */
        sprintf(buf, "%cBEUIP%s", code, pseudo);
        taille = strlen(buf);
    }
    return sendto(sid, buf, taille, 0, (struct sockaddr *)dest, sizeof(*dest));
}
