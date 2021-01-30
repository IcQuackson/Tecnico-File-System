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
#include "assert.h"

#define MAX_COMMANDS 10
#define MAX_INPUT_SIZE 100

char inputCommands[MAX_COMMANDS][MAX_INPUT_SIZE];
int numberCommands = 0;
int headQueue = 0;

char *inputFile = NULL;
char *outputFile = NULL;
int numberThreads = 0;
char *synchStrategy = NULL;
/* Produtor/Consumidor */
pthread_mutex_t mutex;
pthread_mutex_t doneMutex;
pthread_cond_t notFull, notEmpty;
int prodptr = 0;
int consptr = 0;
int done = 0;


int insertCommand(char* data) {
    if (numberCommands != MAX_COMMANDS) {
        numberCommands++;
        strcpy(inputCommands[prodptr], data);
        prodptr ++; if (prodptr == MAX_COMMANDS) prodptr = 0;
        return 1;
    }
    return 0;
}

char * removeCommand() {
    if (numberCommands > 0){
        numberCommands--;
        headQueue = consptr;
        consptr ++; if (consptr == MAX_COMMANDS) consptr = 0;
        return inputCommands[headQueue];
    }
    return NULL;
}

static void displayUsage (const char* appName){
    printf("Usage: %s input_filepath output_filepath threads_number\n", appName);
    exit(EXIT_FAILURE);
}

static void parseArgs (int argc, char* const argv[]){

    if (argc != 4){
        fprintf(stderr, "Invalid input:\n");
        displayUsage(argv[0]);
    }
    inputFile = argv[1];
    outputFile = argv[2];
    numberThreads = atoi(argv[3]);

    if (numberThreads <= 0) {
        fprintf(stderr, "Invalid number of threads\n");
        displayUsage(argv[0]);
    }
}


void errorParse(){
    fprintf(stderr, "Error: command invalid\n");
    exit(EXIT_FAILURE);
}

void * processInput(){
    FILE *file;
    file = fopen(inputFile, "a");
    /* q: condicao de paragem para as tarefas consumidoras */
    fprintf(file, "q");
    fclose(file);
    file = fopen(inputFile, "r");

     if (!file){
        fprintf(stderr, "Error opening file %s\n", inputFile);
        return NULL;
    }

    char line[MAX_INPUT_SIZE];
    while (fgets(line, sizeof(line)/sizeof(char), file)) {
        char token, type;
        char name[MAX_INPUT_SIZE];

        int numTokens = sscanf(line, "%c %s %c", &token, name, &type);
        

        if (pthread_mutex_lock(&mutex) != 0){
            perror("Error: Cannot lock mutex.");
        }
        /* Esperar enquanto buffer esta cheio */
        
        while (numberCommands == MAX_COMMANDS){
            pthread_cond_wait(&notFull, &mutex);
        }
        
        /* perform minimal validation */
        if (numTokens < 1) {
            continue;
        }
        switch (token) {
            case 'c':
                if (numTokens != 3){
                    errorParse();
                }
                if (insertCommand(line))
                    break;
                return NULL;

            case 'l':
                if (numTokens != 2){
                    errorParse();
                }
                if (insertCommand(line))
                    break;
                return NULL;

            case 'd':
                if (numTokens != 2)
                    errorParse();
                if (insertCommand(line))
                    break;
                return NULL;

            case '#':
                break;

            case 'q':
                if (numTokens != 1)
                    errorParse();
                if (insertCommand(line))
                    break;

            default: { /* error */
                errorParse();
            }
        }
        pthread_cond_signal(&notEmpty);
        if (pthread_mutex_unlock(&mutex) != 0){
            perror("Error: Cannot unlock mutex.");
        }
    }
    fclose(file);
    pthread_mutex_lock(&doneMutex);
    done = 1;
    pthread_mutex_unlock(&doneMutex);
    return NULL;
}


void * applyCommands(){
    while(1){

        if (pthread_mutex_lock(&mutex) != 0){
            perror("Error: Cannot lock mutex.");
        }
        //Esperar enquanto buffer vazio
        while (numberCommands == 0){
            if (pthread_mutex_lock(&doneMutex) != 0){
                perror("Error: Cannot lock mutex.");
            }
            if (done == 1){
                return NULL;
            }
            if (pthread_mutex_unlock(&doneMutex) != 0){
                perror("Error: Cannot unlock mutex.");
            }
            pthread_cond_wait(&notEmpty, &mutex);
        } 

        const char* command = removeCommand();
        if (command == NULL){
            continue;
        }

        char token, type;
        char name[MAX_INPUT_SIZE];
        int numTokens = sscanf(command, "%c %s %c", &token, name, &type);

        if (numTokens < 2 && token != 'q') {
            fprintf(stderr, "Error: invalid command in Queue\n");
            return NULL;
        }

        int searchResult;
        switch (token) {
            case 'c':
                switch (type) {
                    case 'f':
                        printf("Create file: %s\n", name);
                        create(name, T_FILE);
                        break;
                    case 'd':
                        printf("Create directory: %s\n", name);
                        create(name, T_DIRECTORY);
                        break;
                    default:
                        fprintf(stderr, "Error: invalid node type\n");
                        return NULL;
                }
                break;

            case 'l':
                searchResult = lookup(name, READ, NULL);
                if (searchResult >= 0)
                    printf("Search: %s found\n", name);
                else
                    printf("Search: %s not found\n", name);
                break;

            case 'd':
                printf("Delete: %s\n", name);
                delete(name);
                break;

            case 'q':
                insertCommand((char *)command);
                pthread_mutex_unlock(&mutex);
                return NULL;

            default: { /* error */
                fprintf(stderr, "Error: command to apply\n");
                return NULL;
            }
        }
        pthread_cond_signal(&notFull);
        if (pthread_mutex_unlock(&mutex) != 0){
            perror("Error: Cannot unlock mutex.");
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

    pthread_t *threads = (pthread_t*) malloc ((numberThreads + 1) * sizeof(pthread_t));

    TIMER_READ(start);
    int err;
    for (int i = 0; i < numberThreads + 1; i++){
        if (i == 0)
            err = pthread_create(&threads[i], NULL, processInput, NULL);
        else{
            err = pthread_create(&threads[i], NULL, applyCommands, NULL);    
        }
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

    TIMER_READ(stop);
    fprintf(timeFile,"TecnicoFS completed in %.4f seconds.\n", TIMER_DIFF_SECONDS(start, stop));
    free(threads);
}

int main(int argc, char* argv[]) {

    /* init filesystem */
    init_fs();
    parseArgs(argc, argv);
    pthread_mutex_init(&mutex, NULL);
    pthread_mutex_init(&doneMutex, NULL);
    FILE * output_file = openOutputFile();


    /* creates thread pool */
    poolThreads(stdout);
    print_tecnicofs_tree(output_file);
    fflush(output_file);
    fclose(output_file);

    pthread_mutex_destroy(&mutex);
    pthread_mutex_destroy(&doneMutex);
    /* release allocated memory */
    destroy_fs();
    exit(EXIT_SUCCESS);
}
