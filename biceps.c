#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <pthread.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include "gescom.h"
#include "creme.h"

#define HIST_FILE ".biceps_history"
#define MAX_MSGS 50
#define BCAST_IP "192.168.88.255"
#define PORT_TCP 9998

/* definition de la structure de liste chainee (conformite tp3 prof) */
struct user_node {
    unsigned long ip;
    char pseudo[LBUF];
    struct user_node *next;
};

/* variables globales synchronisees */
struct user_node *user_list = NULL;
pthread_mutex_t mutex_table = PTHREAD_MUTEX_INITIALIZER;

char boite_reception[MAX_MSGS][LBUF+1];
int nb_msg_attente = 0;
pthread_mutex_t mutex_msg = PTHREAD_MUTEX_INITIALIZER;

pthread_t thread_serveur_udp;
pthread_t thread_serveur_tcp;
int serveur_actif = 0;
int sid_udp = -1;
int sid_tcp = -1;
char pseudo_global[LBUF];

/* prototypes */
void * serveur_udp(void * p);
void * serveur_tcp(void * p);

/* fonctions utilitaires reseau et fichiers */
char * addrip(unsigned long a) {
    static char b[16];
    sprintf(b,"%u.%u.%u.%u", (unsigned int)(a>>24&0xFF), (unsigned int)(a>>16&0xFF),
           (unsigned int)(a>>8&0xFF), (unsigned int)(a&0xFF));
    return b;
}

void verifier_repertoire(const char *chemin) {
    struct stat st = {0};
    if (stat(chemin, &st) == -1) mkdir(chemin, 0700);
}

/* fonctions de gestion de la liste chainee memoire */
void add_user(unsigned long ip, const char *pseudo) {
    struct user_node *new_node = malloc(sizeof(struct user_node));
    if (!new_node) return;
    new_node->ip = ip;
    strncpy(new_node->pseudo, pseudo, LBUF-1);
    new_node->pseudo[LBUF-1] = '\0';
    new_node->next = user_list;
    user_list = new_node;
}

void remove_user(unsigned long ip) {
    struct user_node *curr = user_list, *prev = NULL;
    while (curr) {
        if (curr->ip == ip) {
            if (prev) prev->next = curr->next;
            else user_list = curr->next;
            free(curr);
            return;
        }
        prev = curr;
        curr = curr->next;
    }
}

void free_user_list(void) {
    struct user_node *curr = user_list, *tmp;
    while (curr) {
        tmp = curr;
        curr = curr->next;
        free(tmp);
    }
    user_list = NULL;
}

unsigned long find_ip_by_pseudo(const char *pseudo) {
    struct user_node *curr;
    unsigned long ip = 0;
    pthread_mutex_lock(&mutex_table);
    for (curr = user_list; curr != NULL; curr = curr->next) {
        if (strcmp(curr->pseudo, pseudo) == 0) { ip = curr->ip; break; }
    }
    pthread_mutex_unlock(&mutex_table);
    return ip;
}

/* commandes de base beuip */
void beuip_list(void) {
    struct user_node *curr;
    pthread_mutex_lock(&mutex_table);
    for (curr = user_list; curr != NULL; curr = curr->next) {
        printf("%s : %s\n", addrip(curr->ip), curr->pseudo);
    }
    pthread_mutex_unlock(&mutex_table);
}

void beuip_message(int n, char *p[]) {
    char msg[LBUF] = "";
    struct user_node *curr;
    struct sockaddr_in dest;
    char msg_out[LBUF+1];
    int i;
    
    for (i = 3; i < n; i++) {
        strncat(msg, p[i], LBUF - strlen(msg) - 1);
        if (i < n - 1) strncat(msg, " ", LBUF - strlen(msg) - 1);
    }

    pthread_mutex_lock(&mutex_table);
    for (curr = user_list; curr != NULL; curr = curr->next) {
        if (strcmp(p[2], "all") == 0 || strcmp(curr->pseudo, p[2]) == 0) {
            memset(&dest, 0, sizeof(dest));
            dest.sin_family = AF_INET;
            dest.sin_port = htons(PORT_BEUIP);
            dest.sin_addr.s_addr = htonl(curr->ip);
            snprintf(msg_out, LBUF, "9BEUIP%s", msg);
            sendto(sid_udp, msg_out, strlen(msg_out), 0, (struct sockaddr *)&dest, sizeof(dest));
            if (strcmp(p[2], "all") != 0) break;
        }
    }
    pthread_mutex_unlock(&mutex_table);
}

/* section 3.2 : recuperation de la liste du repertoire via tcp */
void demandeListe(char *pseudo) {
    unsigned long ip = find_ip_by_pseudo(pseudo);
    if (!ip) { printf("erreur : pseudo %s introuvable.\n", pseudo); return; }

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in sin;
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port = htons(PORT_TCP);
    sin.sin_addr.s_addr = htonl(ip);

    if (connect(sock, (struct sockaddr*)&sin, sizeof(sin)) == 0) {
        write(sock, "L", 1);
        char buffer[1024];
        int n;
        printf("--- fichiers de %s ---\n", pseudo);
        while ((n = read(sock, buffer, sizeof(buffer)-1)) > 0) {
            buffer[n] = '\0';
            printf("%s", buffer);
        }
        printf("----------------------\n");
    } else {
        perror("connect");
    }
    close(sock);
}

/* section 3.3 : telechargement de fichier via tcp */
void demandeFichier(char *pseudo, char *nomfic) {
    char savepath[512];
    snprintf(savepath, sizeof(savepath), "reppub/%s", nomfic);
    if (access(savepath, F_OK) == 0) {
        printf("erreur : le fichier %s existe deja localement.\n", nomfic);
        return;
    }

    unsigned long ip = find_ip_by_pseudo(pseudo);
    if (!ip) { printf("erreur : pseudo %s introuvable.\n", pseudo); return; }

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in sin;
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port = htons(PORT_TCP);
    sin.sin_addr.s_addr = htonl(ip);

    if (connect(sock, (struct sockaddr*)&sin, sizeof(sin)) == 0) {
        char req[512];
        int req_len = snprintf(req, sizeof(req), "F%s\n", nomfic);
        write(sock, req, req_len);

        FILE *out = fopen(savepath, "wb");
        if (out) {
            char buffer[1024];
            int n;
            int total = 0;
            while ((n = read(sock, buffer, sizeof(buffer))) > 0) {
                fwrite(buffer, 1, n, out);
                total += n;
            }
            fclose(out);
            if (total > 0) printf("fichier %s telecharge avec succes (%d octets).\n", nomfic, total);
            else { printf("erreur : le fichier %s est introuvable ou vide sur le serveur distant.\n", nomfic); remove(savepath); }
        }
    } else {
        perror("connect");
    }
    close(sock);
}

/* cycle de vie des serveurs */
int beuip_start(char *pseudo) {
    struct sockaddr_in sock_conf, bcast_addr;
    char msg_out[LBUF+1];
    int opt = 1;

    if (serveur_actif) return 1;
    strncpy(pseudo_global, pseudo, LBUF-1);
    pseudo_global[LBUF-1] = '\0';
    verifier_repertoire("reppub");

    /* initialisation udp */
    if ((sid_udp = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) return 1;
    memset(&sock_conf, 0, sizeof(sock_conf));
    sock_conf.sin_family = AF_INET;
    sock_conf.sin_port = htons(PORT_BEUIP);
    sock_conf.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(sid_udp, (struct sockaddr *)&sock_conf, sizeof(sock_conf)) == -1) return 1;
    setsockopt(sid_udp, SOL_SOCKET, SO_BROADCAST, &opt, sizeof(opt));

    /* annonce de presence sur le broadcast */
    memset(&bcast_addr, 0, sizeof(bcast_addr));
    bcast_addr.sin_family = AF_INET;
    bcast_addr.sin_port = htons(PORT_BEUIP);
    bcast_addr.sin_addr.s_addr = inet_addr(BCAST_IP);
    snprintf(msg_out, LBUF, "1BEUIP%s", pseudo_global);
    sendto(sid_udp, msg_out, strlen(msg_out), 0, (struct sockaddr *)&bcast_addr, sizeof(bcast_addr));

    /* lancement des deux threads serveurs */
    serveur_actif = 1;
    pthread_create(&thread_serveur_udp, NULL, serveur_udp, NULL);
    pthread_create(&thread_serveur_tcp, NULL, serveur_tcp, NULL);
    return 1;
}

int beuip_stop(void) {
    struct sockaddr_in dest;
    char msg_out[LBUF+1];
    struct user_node *curr;

    if (!serveur_actif) return 1;
    serveur_actif = 0; /* signale aux threads de s arreter */

    /* depart propre via unicast udp */
    pthread_mutex_lock(&mutex_table);
    for (curr = user_list; curr != NULL; curr = curr->next) {
        memset(&dest, 0, sizeof(dest));
        dest.sin_family = AF_INET;
        dest.sin_port = htons(PORT_BEUIP);
        dest.sin_addr.s_addr = htonl(curr->ip);
        snprintf(msg_out, LBUF, "0BEUIP%s", pseudo_global);
        sendto(sid_udp, msg_out, strlen(msg_out), 0, (struct sockaddr *)&dest, sizeof(dest));
    }
    free_user_list();
    pthread_mutex_unlock(&mutex_table);

    close(sid_udp);
    
    /* connexion factice pour debloquer le accept du serveur tcp */
    int dummy = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in sin;
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port = htons(PORT_TCP);
    sin.sin_addr.s_addr = inet_addr("127.0.0.1");
    connect(dummy, (struct sockaddr*)&sin, sizeof(sin));
    close(dummy);

    pthread_join(thread_serveur_udp, NULL);
    pthread_join(thread_serveur_tcp, NULL);
    return 1;
}

/* routeur de commandes */
int CommandeBEUIP(int n, char *p[]) {
    if (n < 2) return 1;
    if (strcmp(p[1], "start") == 0 && n >= 3) return beuip_start(p[2]);
    if (strcmp(p[1], "stop") == 0) return beuip_stop();
    if (strcmp(p[1], "list") == 0) { beuip_list(); return 1; }
    if (strcmp(p[1], "message") == 0 && n >= 4) { beuip_message(n, p); return 1; }
    if (strcmp(p[1], "ls") == 0 && n >= 3) { demandeListe(p[2]); return 1; }
    if (strcmp(p[1], "get") == 0 && n >= 4) { demandeFichier(p[2], p[3]); return 1; }
    return 1;
}

/* logique d expediteur tcp appelee par le thread tcp (sections 3.2 et 3.3) */
void envoiContenu(int fd) {
    char cmd;
    if (read(fd, &cmd, 1) <= 0) return;

    if (cmd == 'L') {
        if (fork() == 0) {
            dup2(fd, 1); dup2(fd, 2); close(fd);
            execlp("ls", "ls", "-l", "reppub/", NULL);
            exit(1);
        }
        wait(NULL); /* evite les processus zombies */
    } 
    else if (cmd == 'F') {
        char nomfic[256];
        int i = 0;
        while (read(fd, &nomfic[i], 1) > 0 && nomfic[i] != '\n' && i < 255) i++;
        nomfic[i] = '\0';
        if (fork() == 0) {
            dup2(fd, 1); dup2(fd, 2); close(fd);
            char path[512];
            snprintf(path, sizeof(path), "reppub/%s", nomfic);
            execlp("cat", "cat", path, NULL);
            exit(1);
        }
        wait(NULL);
    }
}

/* thread tcp sur le port 9998 (section 3.1) */
void * serveur_tcp(void * p) {
    int fd;
    struct sockaddr_in sin;
    int opt = 1;

    if ((sid_tcp = socket(AF_INET, SOCK_STREAM, 0)) < 0) return NULL;
    setsockopt(sid_tcp, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port = htons(PORT_TCP);
    sin.sin_addr.s_addr = htonl(INADDR_ANY);
    
    if (bind(sid_tcp, (struct sockaddr*)&sin, sizeof(sin)) == -1) { close(sid_tcp); return NULL; }
    listen(sid_tcp, 5);

    while (serveur_actif) {
        fd = accept(sid_tcp, NULL, NULL);
        if (fd >= 0) {
            if (serveur_actif) envoiContenu(fd);
            close(fd);
        }
    }
    close(sid_tcp);
    return NULL;
}

/* thread udp pour l intercommunication beuip */
void * serveur_udp(void * p) {
    int nb_recv;
    struct sockaddr_in sock;
    socklen_t ls;
    char buf[LBUF+1], msg_out[LBUF+1];
    unsigned long sender_ip;

    for (;;) {
        ls = sizeof(sock);
        nb_recv = recvfrom(sid_udp, (void*)buf, LBUF, 0, (struct sockaddr *)&sock, &ls);
        if (nb_recv <= 0) break;
        
        if (nb_recv >= 6 && strncmp(buf + 1, "BEUIP", 5) == 0) {
            if (buf[0] != '0' && buf[0] != '1' && buf[0] != '2' && buf[0] != '9') continue;
            
            pthread_mutex_lock(&mutex_table);
            sender_ip = ntohl(sock.sin_addr.s_addr);
            
            if (buf[0] == '0') {
                remove_user(sender_ip);
            }
            else if (buf[0] == '9') {
                buf[nb_recv] = '\0';
                char pseudo_exp[LBUF] = "inconnu";
                struct user_node *curr;
                for (curr = user_list; curr != NULL; curr = curr->next) {
                    if (curr->ip == sender_ip) { strcpy(pseudo_exp, curr->pseudo); break; }
                }
                pthread_mutex_lock(&mutex_msg);
                if (nb_msg_attente < MAX_MSGS) {
                    snprintf(boite_reception[nb_msg_attente], LBUF, "message de %s : %s", pseudo_exp, buf + 6);
                    nb_msg_attente++;
                }
                pthread_mutex_unlock(&mutex_msg);
            }
            else if (buf[0] == '1' || buf[0] == '2') {
                buf[nb_recv] = '\0';
                int known = 0;
                struct user_node *curr;
                for (curr = user_list; curr != NULL; curr = curr->next) {
                    if (curr->ip == sender_ip && strcmp(curr->pseudo, buf + 6) == 0) { known = 1; break; }
                }
                if (!known) {
                    add_user(sender_ip, buf + 6);
                }
                if (buf[0] == '1') {
                    snprintf(msg_out, LBUF, "2BEUIP%s", pseudo_global);
                    sendto(sid_udp, msg_out, strlen(msg_out), 0, (struct sockaddr *)&sock, ls);
                }
            }
            pthread_mutex_unlock(&mutex_table);
        }
    }
    return NULL;
}

char* creer_prompt(void) {
    char* user = getenv("USER");
    char hostname[256];
    char suffixe = (geteuid() == 0) ? '#' : '$';
    int taille;
    char* prompt;

    if (user == NULL) user = "inconnu";
    if (gethostname(hostname, sizeof(hostname)) != 0) strcpy(hostname, "machine");
    
    taille = snprintf(NULL, 0, "%s@%s%c ", user, hostname, suffixe);
    prompt = malloc(taille + 1);
    if (prompt != NULL) snprintf(prompt, taille + 1, "%s@%s%c ", user, hostname, suffixe);
    
    return prompt;
}

int Sortie(int n, char *p[]) {
    write_history(HIST_FILE);
    clear_history();
    if (serveur_actif) beuip_stop();
    exit(0);
    return 0;
}

int CommandeCD(int n, char *p[]) {
    if (n < 2) { char *home = getenv("HOME"); if (home && chdir(home) != 0) perror("cd"); } 
    else if (chdir(p[1]) != 0) perror("cd");
    return 1;
}

int CommandePWD(int n, char *p[]) {
    char cwd[1024];
    if (getcwd(cwd, sizeof(cwd)) != NULL) printf("%s\n", cwd);
    else perror("pwd");
    return 1;
}

int CommandeVERS(int n, char *p[]) {
    printf("biceps tp3 complet - tcp integre\n");
    return 1;
}

void majComInt(void) {
    ajouteCom("exit", Sortie);
    ajouteCom("cd", CommandeCD);
    ajouteCom("pwd", CommandePWD);
    ajouteCom("vers", CommandeVERS);
    ajouteCom("beuip", CommandeBEUIP);
}

int main(int argc, char *argv[]) {
    char *ligne;
    char *prompt;
    char *commande_isolee;
    int i, k;

    read_history(HIST_FILE);
    majComInt();

    while (1) {
        pthread_mutex_lock(&mutex_msg);
        if (nb_msg_attente > 0) {
            for (k = 0; k < nb_msg_attente; k++) printf("> %s\n", boite_reception[k]);
            nb_msg_attente = 0;
        }
        pthread_mutex_unlock(&mutex_msg);

        prompt = creer_prompt();
        ligne = readline(prompt);
        free(prompt);

        if (ligne == NULL) Sortie(0, NULL);

        if (strlen(ligne) > 0) {
            add_history(ligne);
            char* ptr_ligne = ligne;
            
            while ((commande_isolee = strsep(&ptr_ligne, ";")) != NULL) {
                if (*commande_isolee != '\0') {
                    char *cmds_pipe[MAXPAR];
                    int nb_pipe = 0;
                    char *ptr_pipe = commande_isolee;
                    
                    while ((cmds_pipe[nb_pipe] = strsep(&ptr_pipe, "|")) != NULL) {
                        if (*cmds_pipe[nb_pipe] != '\0') nb_pipe++;
                    }
                    
                    if (nb_pipe == 1) {
                        analyseCom(cmds_pipe[0]);
                        if (NMots > 0) {
                            if (execComInt(NMots, Mots) == 0) execComExt(Mots);
                            for (i = 0; i < NMots; i++) {
                                if (Mots[i] != NULL) { free(Mots[i]); Mots[i] = NULL; }
                            }
                        }
                    } else if (nb_pipe > 1) {
                        execPipeline(cmds_pipe, nb_pipe);
                    }
                }
            }
        }
        free(ligne);
    }
    return 0;
}