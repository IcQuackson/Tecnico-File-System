#include "tecnicofs-client-api.h"
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdio.h>

#define CLIENT_SOCKET "/tmp/client_socket92"

int sockfd;
struct sockaddr_un serv_addr, client_addr;
socklen_t servlen, clilen;
char *server_path, *client_path;


int setSockAddrUn(char *path, struct sockaddr_un *addr) { //sets socketaddr_un members

    if (addr == NULL)
        return 0;
    bzero((char *)addr, sizeof(struct sockaddr_un));
    addr->sun_family = AF_UNIX;
    strcpy(addr->sun_path, path);

    return SUN_LEN(addr);
}

int sendAndRecv(char *comando){
    int return_value;
    /* Envia comando para servidor */
    if (sendto(sockfd, comando, strlen(comando) + 1, 0, (struct sockaddr *) &serv_addr, servlen) < 0) {
        perror("client: sendto error");
        return EXIT_FAILURE;
	}
    /* Recebe o valor de retorno */
    if (recvfrom(sockfd, &return_value, sizeof(return_value), 0, 0, 0) < 0) {
        perror("client: recvfrom error");
        return EXIT_FAILURE;
    }
    return return_value;
}

int tfsCreate(char *filename, char nodeType) {
    char comando[1024];

    sprintf(comando, "c %s %c", filename, nodeType);

    return sendAndRecv(comando);
}

int tfsDelete(char *path) {
    char comando[1024];

    sprintf(comando, "d %s", path);

	return sendAndRecv(comando);
}

int tfsMove(char *from, char *to) {
	return -1;
}

int tfsLookup(char *path) {
	char comando[1024];

    sprintf(comando, "l %s", path);
    
	return sendAndRecv(comando);
}

int tfsPrint(char *filename){
    char comando[1024];

    sprintf(comando, "p %s", filename);
    
	return sendAndRecv(comando);
}

int tfsMount(char *path) {
	server_path = path;
    if ((sockfd = socket(AF_UNIX, SOCK_DGRAM, 0) ) < 0) {
        perror("client: can't open socket");
        return EXIT_FAILURE;
    }

    unlink(CLIENT_SOCKET);
    clilen = setSockAddrUn (CLIENT_SOCKET, &client_addr);
    if (bind(sockfd, (struct sockaddr *) &client_addr, clilen) < 0) {
        perror("client: bind error");
        return EXIT_FAILURE;
    }
    servlen = setSockAddrUn(server_path, &serv_addr);

    return 0;
}

int tfsUnmount() {
    close(sockfd);
    unlink(CLIENT_SOCKET);

	return 0;
}

