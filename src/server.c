#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>

#define PORT   9999
#define BACKLOG 10
#define BUFFER_SIZE 1024

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
        printf("------------------------\n");
        printf("Client #%d connecté (%s:%d)\n", conn_id, inet_ntoa(conn_add.sin_addr), ntohs(conn_add.sin_port));
        
        char buffer[BUFFER_SIZE];
        ssize_t bytes_read;
        
        bytes_read = recv(connfd, buffer, BUFFER_SIZE - 1, 0);
        
        if (bytes_read < 0) {
            perror("Erreur lors de la lecture");
        } 
        else if (bytes_read == 0) {
            printf("Client #%d a fermé la connexion\n", conn_id);
        } 
        else {
            buffer[bytes_read] = '\0';
            printf("Message reçu (%ld octets) : %s\n", bytes_read, buffer);
            
            // Envoie la reponse "Echo : <message>"
            char response[BUFFER_SIZE];
            buffer[strcspn(buffer, "\n")] = '\0';   //Enleve le saut de ligne a la fin du message
            snprintf(response, sizeof(response), "[Connexion #%d] Echo : <%s>", conn_id, buffer);
            
            ssize_t bytes_sent = send(connfd, response, strlen(response), 0);
            if (bytes_sent < 0) {
                perror("Erreur lors de l'envoi");
            } else {
                printf("Réponse envoyée (%ld octets) : %s\n", bytes_sent, response);
            }
        }
        //Fermeture du socket client
        close(connfd);
        printf("Traitement de client #%d terminé\n", conn_id);
        printf("------------------------\n");
        conn_id += 1;
    }
    return 0;
}
