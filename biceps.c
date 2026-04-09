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
#include "gescom.h"
#include "creme.h"

#define HIST_FILE ".biceps_history"
#define MAX_USERS 255
#define MAX_MSGS 50

/* variables globales partagees */
struct user_info table[MAX_USERS];
int user_count = 0;
pthread_mutex_t mutex_table = PTHREAD_MUTEX_INITIALIZER;

/* variables pour la boite de reception */
char boite_reception[MAX_MSGS][LBUF+1];
int nb_msg_attente = 0;
pthread_mutex_t mutex_msg = PTHREAD_MUTEX_INITIALIZER;

pthread_t thread_serveur;
int serveur_actif = 0;
int sid_global = -1;
char pseudo_global[LBUF];

/* utilitaire de conversion ip */
char * addrip(unsigned long a) {
    static char b[16];
    sprintf(b,"%u.%u.%u.%u", (unsigned int)(a>>24&0xFF), (unsigned int)(a>>16&0xFF),
           (unsigned int)(a>>8&0xFF), (unsigned int)(a&0xFF));
    return b;
}

/* fonction centralisee pour les commandes internes non securisees */
void commande(char octet1, char *message, char *pseudo) {
    int i;
    struct sockaddr_in dest_sock;
    char msg_out[LBUF+1];

    /* protection de la lecture de la table */
    pthread_mutex_lock(&mutex_table);

    if (octet1 == '3') {
        /* affichage de la liste */
        printf("--- table des presents (%d) ---\n", user_count);
        for (i = 0; i < user_count; i++) {
            printf("%s : %s\n", table[i].pseudo, addrip(table[i].ip));
        }
    } 
    else if (octet1 == '4') {
        /* gestion des homonymes et message a un pseudo/ip */
        int nb_matches = 0;
        int last_match_index = -1;
        
        for (i = 0; i < user_count; i++) {
            if (strcmp(addrip(table[i].ip), pseudo) == 0) {
                last_match_index = i;
                nb_matches = 1;
                break;
            } 
            else if (strcmp(table[i].pseudo, pseudo) == 0) {
                last_match_index = i;
                nb_matches++;
            }
        }

        if (nb_matches == 1) {
            bzero(&dest_sock, sizeof(dest_sock));
            dest_sock.sin_family = AF_INET;
            dest_sock.sin_port = htons(PORT_BEUIP);
            dest_sock.sin_addr.s_addr = htonl(table[last_match_index].ip);
            snprintf(msg_out, LBUF, "9BEUIP%s", message);
            sendto(sid_global, msg_out, strlen(msg_out), 0, (struct sockaddr *)&dest_sock, sizeof(dest_sock));
            printf("message envoye a %s (%s).\n", table[last_match_index].pseudo, addrip(table[last_match_index].ip));
        } 
        else if (nb_matches > 1) {
            printf("erreur : %d utilisateurs ont le pseudo '%s'.\n", nb_matches, pseudo);
            printf("veuillez preciser l adresse ip pour envoyer votre message :\n");
            for (i = 0; i < user_count; i++) {
                if (strcmp(table[i].pseudo, pseudo) == 0) {
                    printf("- mess %s %s\n", addrip(table[i].ip), message);
                }
            }
        } 
        else {
            printf("erreur : cible '%s' inconnue.\n", pseudo);
        }
    } 
    else if (octet1 == '5') {
        /* message a tous */
        for (i = 0; i < user_count; i++) {
            bzero(&dest_sock, sizeof(dest_sock));
            dest_sock.sin_family = AF_INET;
            dest_sock.sin_port = htons(PORT_BEUIP);
            dest_sock.sin_addr.s_addr = htonl(table[i].ip);
            snprintf(msg_out, LBUF, "9BEUIP%s", message);
            sendto(sid_global, msg_out, strlen(msg_out), 0, (struct sockaddr *)&dest_sock, sizeof(dest_sock));
        }
        printf("message envoye a tous.\n");
    } 
    else if (octet1 == '0') {
        /* envoi de l avis de depart */
        for (i = 0; i < user_count; i++) {
            bzero(&dest_sock, sizeof(dest_sock));
            dest_sock.sin_family = AF_INET;
            dest_sock.sin_port = htons(PORT_BEUIP);
            dest_sock.sin_addr.s_addr = htonl(table[i].ip);
            snprintf(msg_out, LBUF, "0BEUIP%s", pseudo_global);
            sendto(sid_global, msg_out, strlen(msg_out), 0, (struct sockaddr *)&dest_sock, sizeof(dest_sock));
        }
    }

    pthread_mutex_unlock(&mutex_table);
}

/* code du thread serveur udp */
void * serveur_udp(void * p) {
    int nb_recv, i, j, known;
    struct sockaddr_in sock;
    socklen_t ls;
    char buf[LBUF+1], msg_out[LBUF+1];
    unsigned long sender_ip;
    char sender_pseudo[LBUF];

    for (;;) {
        ls = sizeof(sock);
        nb_recv = recvfrom(sid_global, (void*)buf, LBUF, 0, (struct sockaddr *)&sock, &ls);
        
        if (nb_recv <= 0) break;
        buf[nb_recv] = '\0';
        
        if (nb_recv >= 6 && strncmp(buf + 1, "BEUIP", 5) == 0) {
            
            /* filtrage strict */
            if (buf[0] != '0' && buf[0] != '1' && buf[0] != '2' && buf[0] != '9') {
                fprintf(stderr, "\nalerte securite : requete externe refusee (code %c)\n", buf[0]);
                continue;
            }

            pthread_mutex_lock(&mutex_table);

            if (buf[0] == '0') {
                sender_ip = ntohl(sock.sin_addr.s_addr);
                for (i = 0; i < user_count; i++) {
                    if (table[i].ip == sender_ip) {
#ifdef TRACE
                        printf("\ndepart de : %s\n", table[i].pseudo);
#endif
                        for (j = i; j < user_count - 1; j++) table[j] = table[j+1];
                        user_count--;
                        break;
                    }
                }
            }
            else if (buf[0] == '9') {
                sender_ip = ntohl(sock.sin_addr.s_addr);
                known = 0;
                char pseudo_exp[LBUF] = "inconnu";
                
                for (i = 0; i < user_count; i++) {
                    if (table[i].ip == sender_ip) {
                        strcpy(pseudo_exp, table[i].pseudo);
                        known = 1; break;
                    }
                }
                if (!known) strcpy(pseudo_exp, addrip(sender_ip));

                /* stockage securise dans la boite de reception au lieu d un affichage immediat */
                pthread_mutex_lock(&mutex_msg);
                if (nb_msg_attente < MAX_MSGS) {
                    snprintf(boite_reception[nb_msg_attente], LBUF, "message de %s : %s", pseudo_exp, buf + 6);
                    nb_msg_attente++;
                }
                pthread_mutex_unlock(&mutex_msg);
            }
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
                    printf("\nnouveau contact : %s (%s)\n", sender_pseudo, addrip(sender_ip));
#endif
                }
                if (buf[0] == '1') {
                    snprintf(msg_out, LBUF, "2BEUIP%s", pseudo_global);
                    sendto(sid_global, msg_out, strlen(msg_out), 0, (struct sockaddr *)&sock, ls);
                }
            }
            pthread_mutex_unlock(&mutex_table);
        }
    }
    return NULL;
}

/* generation du texte du prompt */
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

/* gestion du protocole beuip */
int CommandeBEUIP(int n, char *p[]) {
    struct sockaddr_in sock_conf, bcast_addr;
    char msg_out[LBUF+1];
    int opt = 1;

    if (n < 2) {
        fprintf(stderr, "usage : beuip start pseudo | stop\n");
        return 1;
    }

    if (strcmp(p[1], "start") == 0) {
        if (n < 3) {
            fprintf(stderr, "erreur : pseudo manquant\n");
            return 1;
        }
        if (serveur_actif) {
            fprintf(stderr, "erreur : le serveur udp tourne deja\n");
            return 1;
        }

        strcpy(pseudo_global, p[2]);

        if ((sid_global = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
            perror("socket"); return 1;
        }

        bzero(&sock_conf, sizeof(sock_conf));
        sock_conf.sin_family = AF_INET;
        sock_conf.sin_port = htons(PORT_BEUIP);
        sock_conf.sin_addr.s_addr = htonl(INADDR_ANY);
        
        if (bind(sid_global, (struct sockaddr *)&sock_conf, sizeof(sock_conf)) == -1) {
            perror("bind"); close(sid_global); return 1;
        }

        setsockopt(sid_global, SOL_SOCKET, SO_BROADCAST, &opt, sizeof(opt));

        bzero(&bcast_addr, sizeof(bcast_addr));
        bcast_addr.sin_family = AF_INET;
        bcast_addr.sin_port = htons(PORT_BEUIP);
        bcast_addr.sin_addr.s_addr = inet_addr("192.168.88.255");
        
        snprintf(msg_out, LBUF, "1BEUIP%s", pseudo_global);
        sendto(sid_global, msg_out, strlen(msg_out), 0, (struct sockaddr *)&bcast_addr, sizeof(bcast_addr));

        if (pthread_create(&thread_serveur, NULL, serveur_udp, NULL) != 0) {
            perror("pthread_create"); close(sid_global); return 1;
        }
        
        serveur_actif = 1;
        printf("serveur udp demarre (thread)\n");
    } 
    else if (strcmp(p[1], "stop") == 0) {
        if (!serveur_actif) {
            fprintf(stderr, "erreur : aucun serveur actif\n");
            return 1;
        }

        commande('0', NULL, NULL);

        close(sid_global);
        pthread_join(thread_serveur, NULL);
        
        pthread_mutex_lock(&mutex_table);
        user_count = 0;
        pthread_mutex_unlock(&mutex_table);
        
        serveur_actif = 0;
        printf("serveur udp arrete proprement\n");
    }
    return 1;
}

/* commande mess interne simplifiee */
int CommandeMESS(int n, char *p[]) {
    if (!serveur_actif) {
        fprintf(stderr, "erreur : demarrez d abord le reseau\n");
        return 1;
    }

    if (n < 2) {
        fprintf(stderr, "usage : mess list | mess all <msg> | mess <pseudo> <msg>\n");
        return 1;
    }

    if (strcmp(p[1], "list") == 0) {
        commande('3', NULL, NULL);
    } 
    else if (strcmp(p[1], "all") == 0 && n >= 3) {
        commande('5', p[2], NULL);
    } 
    else if (n >= 3) {
        commande('4', p[2], p[1]);
    }

    return 1;
}

/* gestion de la sortie */
int Sortie(int n, char *p[]) {
    write_history(HIST_FILE);
    if (serveur_actif) {
        char *args[] = {"beuip", "stop", NULL};
        CommandeBEUIP(2, args);
    }
    printf("fermeture de biceps. au revoir.\n");
    exit(0);
    return 0;
}

int CommandeCD(int n, char *p[]) {
    if (n < 2) {
        char *home = getenv("HOME");
        if (home) chdir(home);
    } else {
        if (chdir(p[1]) != 0) perror("cd");
    }
    return 1;
}

int CommandePWD(int n, char *p[]) {
    char cwd[1024];
    if (getcwd(cwd, sizeof(cwd)) != NULL) printf("%s\n", cwd);
    else perror("pwd");
    return 1;
}

int CommandeVERS(int n, char *p[]) {
    printf("biceps version 3.0 - multithread\n");
    return 1;
}

void majComInt(void) {
    ajouteCom("exit", Sortie);
    ajouteCom("cd", CommandeCD);
    ajouteCom("pwd", CommandePWD);
    ajouteCom("vers", CommandeVERS);
    ajouteCom("beuip", CommandeBEUIP);
    ajouteCom("mess", CommandeMESS);
}

int main(int argc, char *argv[]) {
    char *ligne;
    char *prompt;
    char *commande_isolee;
    int i, k;

    read_history(HIST_FILE);
    majComInt();

    while (1) {
        /* vidage securise de la boite de reception avant de generer le prompt */
        pthread_mutex_lock(&mutex_msg);
        if (nb_msg_attente > 0) {
            printf("\n--- %d message(s) en attente ---\n", nb_msg_attente);
            for (k = 0; k < nb_msg_attente; k++) {
                printf("> %s\n", boite_reception[k]);
            }
            printf("--------------------------------\n");
            nb_msg_attente = 0;
        }
        pthread_mutex_unlock(&mutex_msg);

        prompt = creer_prompt();
        ligne = readline(prompt);
        free(prompt);

        if (ligne == NULL) {
            printf("\n");
            Sortie(0, NULL);
        }

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