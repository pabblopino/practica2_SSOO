#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>

/* CONST VARS */
const int max_line = 1024;
const int max_commands = 10;
#define max_redirections 3 //stdin, stdout, stderr
#define max_args 15

/* VARS TO BE USED FOR THE STUDENTS */
char * argvv[max_args];
char * filev[max_redirections];
int background = 0; 

/**
 * This function splits a char* line into different tokens based on a given character
 * @return Number of tokens 
 */
int tokenizar_linea(char *linea, char *delim, char *tokens[], int max_tokens) {
    int i = 0;
    char *token = strtok(linea, delim);
    while (token != NULL && i < max_tokens - 1) {
        tokens[i++] = token;
        token = strtok(NULL, delim);
    }
    tokens[i] = NULL;
    return i;
}

/**
 * This function processes the command line to evaluate if there are redirections. 
 * If any redirection is detected, the destination file is indicated in filev[i] array.
 * filev[0] for STDIN
 * filev[1] for STDOUT
 * filev[2] for STDERR
 */
void procesar_redirecciones(char *args[]) {
    //initialization for every command
    filev[0] = NULL;
    filev[1] = NULL;
    filev[2] = NULL;
    //Store the pointer to the filename if needed.
    //args[i] set to NULL once redirection is processed
    for (int i = 0; args[i] != NULL; i++) {
        if (strcmp(args[i], "<") == 0) {
            filev[0] = args[i+1];
            args[i] = NULL;
            args[i + 1] = NULL;
        } else if (strcmp(args[i], ">") == 0) {
            filev[1] = args[i+1];
            args[i] = NULL;
            args[i + 1] = NULL;
        } else if (strcmp(args[i], "!>") == 0) {
            filev[2] = args[i+1];
            args[i] = NULL; 
            args[i + 1] = NULL;
        }
    }
}

/**
 * This function processes the input command line and returns in global variables: 
 * argvv -- command an args as argv 
 * filev -- files for redirections. NULL value means no redirection. 
 * background -- 0 means foreground; 1 background.
 */
int procesar_linea(char *linea) {
    background = 0;
    char *comandos[max_commands];
    int num_comandos = tokenizar_linea(linea, "|", comandos, max_commands);

    //Check if background is indicated
    if (strchr(comandos[num_comandos - 1], '&')) {
        background = 1;
        char *pos = strchr(comandos[num_comandos - 1], '&'); 
        //remove character 
        *pos = '\0';
    }

    //Finish processing
    for (int i = 0; i < num_comandos; i++) {
        int args_count = tokenizar_linea(comandos[i], " \t\n", argvv, max_args);
        procesar_redirecciones(argvv);

        /********* This piece of code prints the command, args, redirections and background. **********/
        /*********************** REMOVE BEFORE THE SUBMISSION *****************************************/
        /*********************** IMPLEMENT YOUR CODE FOR PROCESSES MANAGEMENT HERE ********************/
        pid_t pid;

        if (num_comandos == 1){ // Caso de comandos simples
            
            if ((pid = fork()) < 0){
                perror("Error al crear el fork");
                exit(4);
            }

            if (pid == 0){ // Lo realiza el hijo
                execvp(argvv[0], argvv);
                exit(5);
            }

            else{ // Lo realiza el padre
                if (!background){
                    // Si el comando es en primer plano, esperamos que finalice el hijo
                    wait(NULL);
                } 
                else{
                    // Si es en background, mostramos el PID y no esperamos
                    printf("Proceso en background con PID: %d\n", pid);
                }
            }
        }
        
        printf("Comando = %s\n", argvv[0]);
        for(int arg = 1; arg < max_args; arg++)
            if(argvv[arg] != NULL)
                printf("Args = %s\n", argvv[arg]); 
                
        printf("Background = %d\n", background);
        if(filev[0] != NULL)
            printf("Redir [IN] = %s\n", filev[0]);
        if(filev[1] != NULL)
            printf("Redir [OUT] = %s\n", filev[1]);
        if(filev[2] != NULL)
            printf("Redir [ERR] = %s\n", filev[2]);
        /**********************************************************************************************/
    }

    return num_comandos;
}

int main(int argc, char *argv[]) {

    /* STUDENTS CODE MUST BE HERE */
    // char example_line[] = "ls -l | grep scripter | wc -l > redir_out.txt &";
    // int n_commands = procesar_linea(example_line);

    /*Empiezo mi código*/
    if (argc != 2){
        perror("El formato de ejecución no es válido: ./scripter.c <nombre_fichero>");
        exit(1);
    }

    char *fichero_entrada = argv[1]; // Asigno el nombre de mi fichero de entrada a una variable
    char buffer[max_line]; // Declaro el buffer con el tamaño máximo de línea
    char *p = buffer; // Declaro mi puntero
    int n_bytes, fd_entrada, num_comandos;
    int primera_linea = 0;

    if ((fd_entrada = open(fichero_entrada, O_RDONLY)) < 0){ // Abro el fichero de entrada y guardo su descriptor de fichero
        perror("Error al abrir el fichero de entrada");
        exit(2);
    } 

    while ((n_bytes = read(fd_entrada, p, 1)) > 0){
        if (*p == '\n' || *p == '\0'){ // Comprobamos si en la lectura se llega a un salto de línea
            *p = '\0'; // Delimitamos el final de la línea en el buffer
            if (primera_linea == 0){ // Comprobamos que la primera línea es correcta
                if (strcmp(buffer, "## Script de SSOO") != 0){
                    close(fd_entrada);
                    perror("La primera línea es distinta de: ## Script de SSOO");
                    exit(3);
                }

                primera_linea = 1; // Marcamos la primera línea ya leída
                p = buffer; // Volvemos a apuntar p al principio del buffer

            }
            else{ 
                printf("buffer linea: %s\n", buffer);
                num_comandos = procesar_linea(buffer); // Procesamos la línea
                printf("El número de comandos es: %d\n", num_comandos);
                p = buffer; // Volvemos a apuntar p al principio del buffer
            }
        }
        else
            p++; // Incrementamos el puntero byte a byte
    }

    return 0;
}