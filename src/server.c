#include <stdio.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#define PORT   9999
#define BACKLOG 10
#define BUFFER_SIZE 1024
#define TIMEOUT 5  // Timeout de 5 secondes pour select()

int main(void) {
    int listenfd, connfd;
    struct sockaddr_in srv;
    struct sockaddr_in conn_add;
    socklen_t conn_add_len = sizeof(conn_add);
    
    // Tableau des descripteurs clients (initialisé à -1)
    int clients[FD_SETSIZE];
    fd_set read_fds;
    int max_fd;  // Descripteur maximum pour select()
    int client_count = 0;  // Nombre de clients actifs
    
    // Initialisation du tableau clients
    for (int i = 0; i < FD_SETSIZE; i++) {
        clients[i] = -1;
    }
    
    // Creation du serveur
    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenfd < 0){
        perror("Erreur lors de la creation du socket");
        return -1;
    }
    
    // Configuration de SO_REUSEADDR
    int reuse = 1;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    
    // Configuration de bind sur le PORT 9999
    srv.sin_family = AF_INET;
    srv.sin_port = htons(PORT);
    srv.sin_addr.s_addr = INADDR_ANY;
    if (bind(listenfd, (struct sockaddr*)&srv, sizeof(srv)) != 0){
        perror("Erreur sur bind()");
        close(listenfd);
        return -1;
    }
    
    // Configuration de listen avec BACKLOG de 10
    if (listen(listenfd, BACKLOG) < 0){
        perror("Erreur sur listen()");
        close(listenfd);
        return -1;
    }
    
    // Message de demarrage
    printf("Serveur demarré sur le port %d (I/O multiplexage avec select())\n", PORT);
    printf("Timeout select(): %d secondes\n\n", TIMEOUT);
    
    // Traitement des connections (un seul thread)
    while (1){
        // Reinitialiser la fd_set
        FD_ZERO(&read_fds);
        FD_SET(listenfd, &read_fds);
        max_fd = listenfd;
        
        // Ajouter tous les clients actifs à la fd_set
        for (int i = 0; i < FD_SETSIZE; i++) {
            int fd = clients[i];
            if (fd != -1) {
                FD_SET(fd, &read_fds);
                if (fd > max_fd) {
                    max_fd = fd;
                }
            }
        }
        
        // Afficher le nombre de descripteurs surveilles
        printf("[SELECT] Descripteurs surveilles: %d (listenfd + %d clients)\n", 
               1 + client_count, client_count);
        
        // Appel a select() avec timeout
        struct timeval tv;
        tv.tv_sec = TIMEOUT;
        tv.tv_usec = 0;
        
        int activity = select(max_fd + 1, &read_fds, NULL, NULL, &tv);
        
        if (activity < 0) {
            perror("Erreur sur select()");
            continue;
        }
        else if (activity == 0) {
            printf("[SELECT] Timeout écoulé (%d secondes) - Aucune activite\n", TIMEOUT);
            continue;
        }
        
        // Verifier si une nouvelle connexion arrive sur listenfd
        if (FD_ISSET(listenfd, &read_fds)) {
            connfd = accept(listenfd, (struct sockaddr*)&conn_add, &conn_add_len);
            if (connfd < 0) {
                perror("Erreur sur accept()");
            } else {
                // Ajouter le nouveau client au tableau
                int i;
                for (i = 0; i < FD_SETSIZE; i++) {
                    if (clients[i] == -1) {
                        clients[i] = connfd;
                        break;
                    }
                }
                
                if (i == FD_SETSIZE) {
                    printf("[Refus] Trop de clients, FD_SETSIZE (%d) atteint\n", FD_SETSIZE);
                    const char* msg = "Serveur saturé, réessayez plus tard.\n";
                    send(connfd, msg, strlen(msg), 0);
                    close(connfd);
                } else {
                    client_count++;
                    
                    char ip[INET_ADDRSTRLEN];
                    inet_ntop(AF_INET, &conn_add.sin_addr, ip, INET_ADDRSTRLEN);
                    printf("[Nouveau] Client connecté (%s:%d) - Total clients: %d\n",
                           ip, ntohs(conn_add.sin_port), client_count);
                    
                    // Envoyer un message de bienvenue
                    char welcome[100];
                    snprintf(welcome, sizeof(welcome), 
                             "Bienvenue ! Clients actifs: %d\n", client_count);
                    send(connfd, welcome, strlen(welcome), 0);
                }
            }
        }
        
        // Verifier l'activite sur les clients existants
        for (int i = 0; i < FD_SETSIZE; i++) {
            int fd = clients[i];
            if (fd != -1 && FD_ISSET(fd, &read_fds)) {
                char buffer[BUFFER_SIZE];
                ssize_t bytes_read = recv(fd, buffer, BUFFER_SIZE - 1, 0);
                
                if (bytes_read <= 0) {
                    // Deconnexion ou erreur
                    if (bytes_read == 0) {
                        printf("[Deconnexion] Client #%d a fermé la connexion\n", fd);
                    } else {
                        perror("Erreur lors de la lecture");
                    }
                    close(fd);
                    clients[i] = -1;
                    client_count--;
                    printf("[Statut] Clients actifs restants: %d\n", client_count);
                } 
                else {
                    // Traiter le message
                    buffer[bytes_read] = '\0';
                    buffer[strcspn(buffer, "\n")] = '\0';
                    
                    printf("[Message] Client %d a envoyé: \"%s\"\n", fd, buffer);
                    
                    // Reponse "Echo: <message>"
                    char response[BUFFER_SIZE + 50];
                    snprintf(response, sizeof(response), "[Client %d] Echo: <%s>\n", fd, buffer);
                    send(fd, response, strlen(response), 0);
                }
            }
        }
    }
    
    // Nettoyage (normalement jamais atteint)
    for (int i = 0; i < FD_SETSIZE; i++) {
        if (clients[i] != -1) {
            close(clients[i]);
        }
    }
    close(listenfd);
    return 0;
}
