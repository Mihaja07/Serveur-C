#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <sys/wait.h>
#include <stdlib.h>

#define PORT   9999
#define BACKLOG 10
#define BUFFER_SIZE 1024
#define COUNTER_FILE "/tmp/serveur_connexions.txt"  // Fichier temporaire

/*Fonction handle_client()
 * Objectif: traiter les connexions client
 * Parametre: le descripteur de fichier de connexion client, le numero sequentiel de client
 * Retour: void, la fonction ne retourne rien
*/
void handle_client(int connfd, int conn_id);
/*Fonction sigchld_handler()
 * Objectif : Éviter les processus zombies en récupérant automatiquement le code de sortie
 * des processus fils lorsqu'ils se terminent.
 * Paramètres : int sig : Le numéro du signal reçu (ici SIGCHLD, envoyé par le noyau quand
 * un fils se termine)
 * Retour: void, la fonction ne retourne rien
*/ 
void sigchld_handler(int sig);
/*Fonction increment_counter()
 * Objectif: augmenter le nombre de connexion actif
 * Parametre: void, la fonction ne prend aucun parametre
 * Retour: void, la fonction ne retourne rien
*/
void increment_counter(void);
/*Fonction decrement_counter()
 * Objectif: diminuer le nombre de connexion actif
 * Parametre: void, la fonction ne prend aucun parametre
 * Retour: void, la fonction ne retourne rien
*/
void decrement_counter(void);
/*Fonction get_active_count()
 * Objectif: retourne le nombre de connection actif
 * Parametre: void, la fonction ne prend aucun parametre
 * Retour: void, la fonction ne retourne rien
*/
int get_active_count(void);
/*Fonction reset_counter()
 * Objectif: Met le compteur de connexions active a 0
 * Parametre: void, la fonction ne prend aucun parametre
 * Retour: void, la fonction ne retourne rien
*/
void reset_counter(void);

int main(void) {
    int listenfd, connfd;
    struct sockaddr_in srv;
    struct sockaddr_in conn_add;
    int conn_id = 1, conn_add_len = sizeof(conn_add);  //conn_id est le numero sequentiel de chaque connexion
    reset_counter(); //Reinitialisation du compteur
    
    // Configuration du gestionnaire SIGCHLD pour éviter les zombies
    struct sigaction sa;
    sa.sa_handler = sigchld_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;  // Réessaie automatiquement les appels interrompus
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("Erreur sigaction");
        return -1;
    }
    
    //Creation du serveur
    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenfd < 0){
        perror("Erreur lors de la creation du socket");
        close(listenfd);
        return -1;
    }
    
    //Configuration de SO_REUSEADDR
    int reuse = 1;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    
    //Configuration de bind sur le PORT 999
    srv.sin_family = AF_INET;
    srv.sin_port = htons(PORT);
    srv.sin_addr.s_addr = INADDR_ANY;  //ecoute sur tous les interfaces
    if (bind(listenfd, (struct sockaddr*)&srv, sizeof(srv)) != 0){
        perror("Erreur sur bind()");
        close(listenfd);
        return -1;
    }
    
    //Configuration de listen avec BACKLOG de 10
    if (listen(listenfd, BACKLOG) < 0){
        perror("Erreur sur listen()");
        close(listenfd);
        return -1;
    }
    //Message de demarrage
    printf("Serveur demarré sur le port %d\n", PORT);
    //Traitement des connections
    while (1){
        printf("En attente de connexion...\n");
        connfd = accept(listenfd, (struct sockaddr*)&conn_add, &conn_add_len);
        if (connfd < 0){
            perror("Erreur sur accept()");
            continue;
        }
        // CRÉATION DU PROCESSUS FILS
        pid_t pid = fork();
        
        if (pid < 0) {
            // Erreur de fork
            perror("Erreur fork()");
            close(connfd);
            continue;
        }
        else if (pid == 0) {
            // ---------- PROCESSUS FILS ----------
            increment_counter(); //Augmenter le nombre de connexion actif
            printf("Nombre de connection actives: %d\n", get_active_count());
            close(listenfd);  // Le fils n'écoute pas les nouvelles connexions
            printf("[Fils PID=%d] Traitement du client #%d\n", getpid(), conn_id);
            handle_client(connfd, conn_id);  // Gérer le client
            decrement_counter();
            printf("[Fils PID=%d] Fin du traitement client #%d\n", getpid(), conn_id);
            close(connfd);
            exit(0);  // Terminer le fils
        }
        else {
            // ---------- PROCESSUS PÈRE ----------
            close(connfd);  // Le père ne parle pas au client (il a sa copie)
        }
        conn_id += 1;
    }
    return 0;
}

// Incrémenter le compteur de connexions actives
void increment_counter(void) {
    FILE *f = fopen(COUNTER_FILE, "r+");
    int count = 0;
    
    if (f) {
        // Lire la valeur actuelle
        fscanf(f, "%d", &count);
        rewind(f);  // Retourner au début du fichier
        count++;
        fprintf(f, "%d", count);
        fclose(f);
    } else {
        // Le fichier n'existe pas, le créer
        f = fopen(COUNTER_FILE, "w");
        if (f) {
            fprintf(f, "1");
            fclose(f);
        } else {
            perror("Erreur increment_counter: impossible d'ouvrir le fichier");
        }
    }
}

// Décrémenter le compteur de connexions actives
void decrement_counter(void) {
    FILE *f = fopen(COUNTER_FILE, "r+");
    int count = 0;
    
    if (f) {
        fscanf(f, "%d", &count);
        rewind(f);
        count--;
        if (count < 0) count = 0;  // Sécurité: ne pas descendre en dessous de 0
        fprintf(f, "%d", count);
        fclose(f);
    } else {
        // Si le fichier n'existe pas, créer avec 0
        f = fopen(COUNTER_FILE, "w");
        if (f) {
            fprintf(f, "0");
            fclose(f);
        }
    }
}

// Lire le compteur actuel
int get_active_count(void) {
    FILE *f = fopen(COUNTER_FILE, "r");
    int count = 0;
    
    if (f) {
        fscanf(f, "%d", &count);
        fclose(f);
    }
    return count;
}

// Réinitialiser le compteur
void reset_counter(void) {
    FILE *f = fopen(COUNTER_FILE, "w");
    if (f) {
        fprintf(f, "0");
        fclose(f);
        printf("[Init] Compteur réinitialisé à 0\n");
    } else {
        perror("Erreur reset_counter");
    }
}

//Gerer les clients
void handle_client(int connfd, int conn_id) {
    char buffer[BUFFER_SIZE];
    ssize_t bytes_read;
    
    bytes_read = recv(connfd, buffer, BUFFER_SIZE - 1, 0);
    
    if (bytes_read < 0) {
        perror("Erreur lors de la lecture");
    } 
    else if (bytes_read == 0) {
        printf("[Fils PID=%d] Client #%d a fermé la connexion\n", getpid(), conn_id);
    } 
    else {
        buffer[bytes_read] = '\0';
        
        printf("[Fils PID=%d] Message reçu (%ld octets) : %s\n", getpid(), bytes_read, buffer);
        
        // Envoie la reponse "Echo : <message>"
        char response[BUFFER_SIZE];
        buffer[strcspn(buffer, "\n")] = '\0';   //Enleve le saut de ligne a la fin du message
        snprintf(response, sizeof(response), "[Connexion #%d] Echo : <%s>", conn_id, buffer);
        
        ssize_t bytes_sent = send(connfd, response, strlen(response), 0);
        if (bytes_sent < 0) {
            perror("Erreur lors de l'envoi");
        } else {
            printf("[Fils PID=%d] Réponse envoyée a client#%d (%ld octets) : %s\n", getpid(), conn_id, bytes_sent, response);
        }
    }
}

void sigchld_handler(int sig) {
    // Waitpid avec WNOHANG : non-bloquant, récupère tous les fils terminés
    pid_t pid;
    int status;
    
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        printf("------------------------------------------------\n");
        printf("[Parent] Signal SIGCHLD reçu - Fils PID=%d terminé\n", pid);
        
        // Optionnel : afficher comment le processus s'est terminé
        if (WIFEXITED(status)) {
            printf(" (code de retour: %d)\n", WEXITSTATUS(status));
        } else if (WIFSIGNALED(status)) {
            printf(" (tué par signal: %d)\n", WTERMSIG(status));
        } else {
            printf("\n");
        }
        printf("Nombre de connection actives: %d\n", get_active_count());
        printf("------------------------------------------------\n");
    }
}
