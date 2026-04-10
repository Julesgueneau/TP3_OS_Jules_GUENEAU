NOM: GUENEAU
PRENOM: Jules

# TP3 - Système d'Exploitation et Réseau : BICEPS V3

## Description du projet
Ce projet implémente BICEPS (Bel Interpréteur de Commandes des Élèves de Polytech Sorbonne), un shell interactif couplé à un client/serveur de messagerie et de partage de fichiers en pair-à-pair reposant sur les protocoles UDP et TCP.

## Architecture et Structure du Code
L'application est conçue autour d'une architecture multi-thread robuste et respecte les contraintes de compilation strictes (-Wall -Werror) :
- **Multi-threading :** Le processus principal gère l'interface utilisateur (via la librairie readline). Deux threads distincts s'exécutent en arrière-plan : un serveur UDP pour la messagerie et le routage, et un serveur TCP (port 9998) dédié au transfert lourd de fichiers.
- **Gestion de la mémoire :** La table des contacts est gérée via une liste chaînée allouée dynamiquement (`struct user_node`). À l'arrêt du programme, l'intégralité de la mémoire allouée est libérée (`free_user_list`, `clear_history`) pour garantir une exécution sans aucune fuite sous Valgrind.
- **Interface non bloquante :** L'affichage des messages réseau entrants est différé grâce à une boîte de réception protégée par mutex (`PTHREAD_MUTEX_INITIALIZER`). Cela empêche l'interruption visuelle du prompt lors de la frappe d'une commande.

## Fonctionnalités Implémentées (Conformité 100%)
- **Cycle de vie :** `beuip start <pseudo>` (initialise les serveurs et broadcast la présence) et `beuip stop` (envoie l'avis de départ et ferme proprement les threads).
- **Annuaire :** `beuip list` affiche la liste exacte des utilisateurs présents.
- **Messagerie (UDP) :** - `beuip message <pseudo> <msg>` : Envoi unicast avec concaténation automatique des chaînes à espaces.
  - `beuip message all <msg>` : Envoi broadcast à tous les pairs de la table.
- **Partage de fichiers P2P (TCP - Bonus Partie 3) :**
  - `beuip ls <pseudo>` : Interroge un pair pour lister le contenu de son répertoire `reppub/` (utilisation de `fork`, `dup2` et `execlp`).
  - `beuip get <pseudo> <fichier>` : Ouvre un flux TCP vers un pair pour rapatrier un fichier ciblé dans le dossier public local.
- **Accusés de réception (Bonus) :** Ajout d'un code protocolaire applicatif ('A') renvoyé silencieusement par le destinataire pour notifier l'expéditeur de la bonne réception d'un message privé.

## Compilation et Tests
- `make` : Construit l'exécutable principal `biceps`.
- `make memory-leak` : Construit le binaire de débogage `biceps-memory-leaks` sans optimisation (-O0) pour l'analyse Valgrind.
- `make clean` : Purge le répertoire de tout binaire ou fichier objet.
