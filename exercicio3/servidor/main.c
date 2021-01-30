#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include <ctype.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/syscall.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/uio.h>
#include "fs/timer.h"
#include "fs/operations.h"
#include "assert.h"
#include <sys/stat.h>

#define MAX_INPUT_SIZE 100
#define MAX_OUTPUT_SIZE 100
#define INDIM 30
#define OUTDIM 512

int numberThreads = 0;
char *socket_path = NULL;
char *output_file = NULL;
socklen_t addrlen;
int sockfd;
pthread_mutex_t global;


static void displayUsage (const char* appName){
    printf("Usage: %s threads_number socket_path\n", appName);
    exit(EXIT_FAILURE);
}


static void parseArgs (int argc, char* const argv[]){

    if (argc != 3){
        fprintf(stderr, "Invalid input:\n");
        displayUsage(argv[0]);
    }
    numberThreads = atoi(argv[1]);
    socket_path = argv[2];

    if (numberThreads <= 0) {
        fprintf(stderr, "Invalid number of threads\n");
        displayUsage(argv[0]);
    }
}


void errorParse(){
    fprintf(stderr, "Error: command invalid\n");
    exit(EXIT_FAILURE);
}


void lockMutex(pthread_mutex_t mutex){
    if (pthread_mutex_lock(&mutex) != 0){
        perror("error: can't lock mutex.\n");
    }
}

void unlockMutex(pthread_mutex_t mutex){
    if (pthread_mutex_unlock(&mutex) != 0){
        perror("error: can't unlock mutex.\n");
    }
}


FILE * openOutputFile(char *filename){
    FILE * file;
    file = fopen(filename, "w");
    if (!file) {
        perror("Error: can't open output file.");
        exit(EXIT_FAILURE);
    }
    return file;
}


int applyCommands(void *command){

    if (command == NULL){
        return EXIT_FAILURE;
    }
    FILE *output_file;
    int res;
    char token, type;
    char name[MAX_INPUT_SIZE];
    int numTokens = sscanf((const char*) command, "%c %s %c", &token, name, &type);

    if (numTokens < 2) {
        fprintf(stderr, "Error: invalid command in Queue\n");
        return EXIT_FAILURE;
    }

    int searchResult;
    switch (token) {
        case 'c':
            switch (type) {
                case 'f':
                    printf("Create file: %s\n", name);
                    lockMutex(global);
                    res = create(name, T_FILE);
                    unlockMutex(global);
                    return res;
                case 'd':
                    printf("Create directory: %s\n", name);
                    lockMutex(global);
                    res = create(name, T_DIRECTORY);
                    unlockMutex(global);
                    return res;
                default:
                    fprintf(stderr, "Error: invalid node type\n");
                    return EXIT_FAILURE;
            }
            break;

        case 'l':
            lockMutex(global);
            searchResult = lookup(name);
            unlockMutex(global);
            if (searchResult >= 0)
                printf("Search: %s found\n", name);
            else
                printf("Search: %s not found\n", name);
            return searchResult;

        case 'd':
            printf("Delete: %s\n", name);
            lockMutex(global);
            res = delete(name);
            unlockMutex(global);
            return res;
        
        case 'p':
            lockMutex(global);

            output_file = openOutputFile(name);
            print_tecnicofs_tree(output_file);

            unlockMutex(global);
            return EXIT_SUCCESS;

        default: { /* error */
            fprintf(stderr, "Error: command to apply\n");
            return EXIT_FAILURE;
        }
    }
    return EXIT_FAILURE;
}


int setSockAddrUn(char *path, struct sockaddr_un *addr) { //sets socketaddr_un members
    if (addr == NULL)
        return 0;

    bzero((char *)addr, sizeof(struct sockaddr_un));
    addr->sun_family = AF_UNIX;
    strcpy(addr->sun_path, path);

    return SUN_LEN(addr);
}


void* recebe_comando(){

    while (1) {
        struct sockaddr_un client_addr;
        char comando[INDIM];
        int c;
        int return_value;

        addrlen = sizeof(struct sockaddr_un);
        c = recvfrom(sockfd, comando, sizeof(comando) - 1, 0,
            (struct sockaddr *) &client_addr, &addrlen); //Recebe mensagem do cliente
        if (c <= 0) continue;

        //Preventivo, caso o cliente nao tenha terminado a mensagem em '\0', 
        comando[c] = '\0';
        printf("Mensagem recebida: %s\n", comando);

        return_value = applyCommands(comando);  //executa comando

        /* Envia resposta ao cliente */
        sendto(sockfd, (const int *) &return_value, sizeof(return_value), 0, (struct sockaddr *) &client_addr, addrlen);
    }
    //Fechar e apagar o nome do socket, apesar deste programa 
    //nunca chegar a este ponto
    close(sockfd);
    unlink(socket_path);
}


void init_socket(){
    struct sockaddr_un server_addr;

    if ((sockfd = socket(AF_UNIX, SOCK_DGRAM, 0)) < 0) {
        perror("server: can't open socket");
        exit(EXIT_FAILURE);
    }

    unlink(socket_path);
    
    addrlen = setSockAddrUn(socket_path, &server_addr);
    if (bind(sockfd, (struct sockaddr *) &server_addr, addrlen) < 0) {
        perror("server: bind error");
        exit(EXIT_FAILURE);
    }
}


void poolThreads(){

    pthread_t *threads = (pthread_t*) malloc (numberThreads * sizeof(pthread_t));

    int err;
    for (int i = 0; i < numberThreads; i++){
        err = pthread_create(&threads[i], NULL, recebe_comando, NULL);
        if (err != 0) {
            perror("Error: can't create thread.");
            exit(EXIT_FAILURE);
        }
    }

    for (int i = 0; i < numberThreads; i++) {
        if (pthread_join(threads[i], NULL)) {
            perror("Error: can't join thread.");
        }
    }
    free(threads);
}

int main(int argc, char* argv[]) {
    
    /* init filesystem */
    init_fs();
    parseArgs(argc, argv);

    /* init mutex */
    if (pthread_mutex_init(&global, NULL) != 0){
        perror("error: can't init mutex");
    }
    
    /* init socket data structs */
    init_socket();
    
    /* creates thread pool */
    poolThreads(stdout);

    /* release allocated memory */
    destroy_fs();
    if (pthread_mutex_destroy(&global) != 0){
        perror("error: can't destroy mutex");
    };
    exit(EXIT_SUCCESS);
}