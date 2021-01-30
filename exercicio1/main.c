#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include <ctype.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/syscall.h>
#include "fs/timer.h"
#include "fs/operations.h"

#define MAX_COMMANDS 150000
#define MAX_INPUT_SIZE 100

char inputCommands[MAX_COMMANDS][MAX_INPUT_SIZE];
int numberCommands = 0;
int headQueue = 0;

char *inputFile = NULL;
char *outputFile = NULL;
int numberThreads = 0;
char *synchStrategy = NULL;
pthread_mutex_t lock ; 
pthread_rwlock_t lock_rw ;



int insertCommand(char* data) {
    if (numberCommands != MAX_COMMANDS) {
        strcpy(inputCommands[numberCommands++], data);
        return 1;
    }
    return 0;
}

char* removeCommand() {
    pthread_rwlock_wrlock(&lock_rw);
    if (numberCommands > 0){
        numberCommands--;
        return inputCommands[headQueue++];  
    }
    pthread_rwlock_unlock(&lock_rw);
    return NULL;
}

static void displayUsage (const char* appName){
    printf("Usage: %s input_filepath output_filepath threads_number synch_Strategy\n", appName);
    exit(EXIT_FAILURE);
}

static void parseArgs (int argc, char* const argv[]){
    
    if (argc != 5){
        fprintf(stderr, "Invalid input:\n");
        displayUsage(argv[0]);
    }
    inputFile = argv[1];
    outputFile = argv[2];
    numberThreads = atoi(argv[3]);
    synchStrategy = argv[4]; 

    if (!numberThreads) {
        fprintf(stderr, "Invalid number of threads\n");
        displayUsage(argv[0]);
    }
}


void errorParse(){
    fprintf(stderr, "Error: command invalid\n");
    exit(EXIT_FAILURE);
}

void processInput(){
    FILE *file;
    file = fopen(inputFile, "r");

     if(!file){
        fprintf(stderr, "Error reading file %s\n", inputFile);
        exit(EXIT_FAILURE);
    }

    char line[MAX_INPUT_SIZE];

    while (fgets(line, sizeof(line)/sizeof(char), file)) {
        char token, type;
        char name[MAX_INPUT_SIZE];

        int numTokens = sscanf(line, "%c %s %c", &token, name, &type);

        /* perform minimal validation */
        if (numTokens < 1) {
            continue;
        }
        switch (token) {
            case 'c':
                if (numTokens != 3)
                    errorParse();
                if (insertCommand(line))
                    break;
                return;
            
            case 'l':
                if (numTokens != 2)
                    errorParse();
                if (insertCommand(line))
                    break;
                return;
            
            case 'd':
                if (numTokens != 2)
                    errorParse();
                if (insertCommand(line))
                    break;
                return;
            
            case '#':
                break;
            
            default: { /* error */
                errorParse();
            }
        }
    }
    fclose(file);
}


void * applyCommands(){
    
    while(1){ 
        pthread_mutex_lock(&lock);
        if(numberCommands > 0){
            const char* command = removeCommand();
            pthread_mutex_unlock(&lock);
            if (command == NULL){
                continue;
            }

            char token, type;
            char name[MAX_INPUT_SIZE];
            int numTokens = sscanf(command, "%c %s %c", &token, name, &type);
            
            if (numTokens < 2) {
                fprintf(stderr, "Error: invalid command in Queue\n");
                exit(EXIT_FAILURE);
            }

            int searchResult;
            switch (token) {
                case 'c':
                    switch (type) {
                        case 'f':
                            pthread_mutex_lock(&lock);
                            printf("Create file: %s\n", name);
                            create(name, T_FILE);
                            pthread_mutex_unlock(&lock);
                            break;
                        case 'd':
                            pthread_mutex_lock(&lock);
                            printf("Create directory: %s\n", name);
                            create(name, T_DIRECTORY);
                            pthread_mutex_unlock(&lock);
                            break;
                        default:
                            fprintf(stderr, "Error: invalid node type\n");
                            exit(EXIT_FAILURE);
                    }
                    break;
                case 'l': 
                    pthread_mutex_lock(&lock);
                    searchResult = lookup(name);
                    if (searchResult >= 0)
                        printf("Search: %s found\n", name);
                    else
                        printf("Search: %s not found\n", name);
                    pthread_mutex_unlock(&lock);
                    break;
                case 'd':
                    pthread_mutex_lock(&lock);
                    printf("Delete: %s\n", name);
                    delete(name);
                    pthread_mutex_unlock(&lock);
                    break;
                default: { /* error */
                    fprintf(stderr, "Error: command to apply\n");
                    exit(EXIT_FAILURE);
                }
            }
        }else{
            pthread_mutex_unlock(&lock);
            return NULL;
            }
    }
}

FILE * openOutputFile(){
    FILE * file = malloc(sizeof(FILE));
    file = fopen(outputFile, "w");
    if (!file) {
        perror("Error: can't open output file.");
        exit(EXIT_FAILURE);
    }
    return file;
}


void poolThreads(FILE * timeFile){
    TIMER_T start, stop;

    pthread_t *slaves = (pthread_t*) malloc (numberThreads * sizeof(pthread_t));

    TIMER_READ(start);
    for (int i = 0; i < numberThreads; i++){
        int err = pthread_create(&slaves[i], NULL, applyCommands, NULL);
        if (err != 0) {
            perror("Error: can't create thread.");
            exit(EXIT_FAILURE);
        }
    }

    for (int i = 0; i < numberThreads; i++) {
        if (pthread_join(slaves[i], NULL)) {
            perror("Error: can't join thread.");
        }
    }
    applyCommands(); 
    
    TIMER_READ(stop);
    fprintf(timeFile,"TecnicoFS completed in %.4f seconds.\n", TIMER_DIFF_SECONDS(start, stop));
    free(slaves);
}

int main(int argc, char* argv[]) {
    
    /* init filesystem */
    init_fs();
    parseArgs(argc, argv);
    processInput();
    FILE * output_file = openOutputFile(); 
    pthread_mutex_init(&lock, NULL);
    
    /* creates thread pool */
    poolThreads(stdout);
    print_tecnicofs_tree(output_file);
    fflush(output_file);
    fclose(output_file);

    pthread_mutex_destroy(&lock);
    /* release allocated memory */
    destroy_fs();
    exit(EXIT_SUCCESS);
}
