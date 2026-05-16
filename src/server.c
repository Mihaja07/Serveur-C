#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <errno.h>

#define PORT   9999
#define BACKLOG 10
#define BUFFER_SIZE 1024
#define MAX_THREADS 16

static int connexions_actives = 0;
static pthread_mutex_t mutex_connexions = PTHREAD_MUTEX_INITIALIZER;

// Structure pour passer les paramètres au thread
typedef struct {
    int connfd;
    int conn_id;
    char client_ip[INET_ADDRSTRLEN];
    int client_port;
} client_info_t;

/*Fonction handle_client_thread()
 * Objectif: traiter les connexions client
 * Parametre:
 * Retour:
*/
void* handle_client_thread(void* arg);
/*Fonction afficher_statut()
 * Objectif: Affiche le nombre de connexion active
 * Parametre: void
 * Retour: void
*/
void afficher_statut(void);
/*Fonction incremeter_connexions()
 * Objectif: incremente le nombre de connexion active
 * Parametre: void
 * Retour: void
*/
void incrementer_connexions(void);
/*Fonction decrementer_connexions()
 * Objectif: Decremente le nombre de connexion active
 * Parametre: void
 * Retour: void
*/
void decrementer_connexions(void);
/*Fonction get_connexions_actives()
 * Objectif: retourne le nombre de connexion active
 * Parametre: void
 * Retour: int, le nombre de connexions actives
*/
int get_connexions_actives(void);

int main(void) {
    int listenfd, connfd;
    struct sockaddr_in srv;
    struct sockaddr_in conn_add;
    int conn_id = 1, conn_add_len = sizeof(conn_add);  //conn_id est le numero sequentiel de chaque connexion
    
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
    
    //Configuration de bind sur le PORT 9999
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

        // Vérifier si le pool n'est pas saturé
        if (get_connexions_actives() >= MAX_THREADS) {
            printf("[Refus] Pool saturé (%d/%d threads actifs) - Connexion rejetée\n", 
                   get_connexions_actives(), MAX_THREADS);
            const char* msg = "Serveur saturé, réessayez plus tard.\n";
            send(connfd, msg, strlen(msg), 0);
            close(connfd);
            continue;
        }

        // Préparer les infos du client
        client_info_t* client_info = malloc(sizeof(client_info_t));
        if (!client_info) {
            perror("malloc");
            close(connfd);
            continue;
        }
        client_info->connfd = connfd;
        client_info->conn_id = conn_id;
        inet_ntop(AF_INET, &conn_add.sin_addr, client_info->client_ip, INET_ADDRSTRLEN);
        client_info->client_port = ntohs(conn_add.sin_port);
        conn_id++;
        incrementer_connexions();

        // Créer le thread
        pthread_t thread;
        int ret = pthread_create(&thread, NULL, handle_client_thread, client_info);
        if (ret != 0) {
            perror("Erreur sur pthread_create()");
            decrementer_connexions();
            free(client_info);
            close(connfd);
            continue;
        }

        //Detacher le thread
        pthread_detach(thread);
        afficher_statut();
    }
    close(listenfd);
    return 0;
}

void incrementer_connexions(void) {
    pthread_mutex_lock(&mutex_connexions);
    connexions_actives++;
    pthread_mutex_unlock(&mutex_connexions);
}

void decrementer_connexions(void) {
    pthread_mutex_lock(&mutex_connexions);
    connexions_actives--;
    pthread_mutex_unlock(&mutex_connexions);
}

int get_connexions_actives(void) {
    int value;
    pthread_mutex_lock(&mutex_connexions);
    value = connexions_actives;
    pthread_mutex_unlock(&mutex_connexions);
    return value;
}

void afficher_statut(void) {
    pthread_mutex_lock(&mutex_connexions);
    printf("[STATUS] Connexions actives: %d/%d\n", connexions_actives, MAX_THREADS);
    pthread_mutex_unlock(&mutex_connexions);
}

void* handle_client_thread(void* arg) {
    // Récupérer les informations du client
    client_info_t* info = (client_info_t*)arg;
    int connfd = info->connfd;
    int conn_id = info->conn_id;
    char* client_ip = info->client_ip;
    int client_port = info->client_port;
    
    // Libérer la mémoire allouée pour les paramètres
    free(info);
    
    printf("[Thread %lu] Traitement client #%d (%s:%d)\n",
           pthread_self(), conn_id, client_ip, client_port);
    
    char buffer[BUFFER_SIZE];
    ssize_t bytes_read;
    
    bytes_read = recv(connfd, buffer, BUFFER_SIZE - 1, 0);
        
    if (bytes_read < 0) {
        perror("Erreur lors de la lecture");
    } 
        
    else if (bytes_read == 0) {
        printf("[Thread %lu] Client #%d (%s:%d) a fermé la connexion\n",
            pthread_self(), conn_id, client_ip, client_port);
    } 
        
    else {
        buffer[bytes_read] = '\0';
        buffer[strcspn(buffer, "\n")] = '\0';
            
        printf("[Thread %lu] Client #%d a envoyé: \"%s\"\n", 
                   pthread_self(), conn_id, buffer);
            
        char response[BUFFER_SIZE];
        snprintf(response, sizeof(response), "[Connexion #%d] Echo: <%s>\n", conn_id, buffer); 
        send(connfd, response, strlen(response), 0);
    }
    
    close(connfd); 
    decrementer_connexions();
    printf("[Thread %lu] Fin traitement client #%d\n", pthread_self(), conn_id);
    
    return NULL;
}
