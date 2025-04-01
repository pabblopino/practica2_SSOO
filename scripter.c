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
int contador_zombies = 0;
pid_t pids_zombies[10];

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
    int j = 0;
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
            i++; // Saltamos otra vez para llegar al siguiente argumento
        } else if (strcmp(args[i], ">") == 0) {
            filev[1] = args[i+1];
            args[i] = NULL;
            args[i + 1] = NULL;
            i++;
        } else if (strcmp(args[i], "!>") == 0) {
            filev[2] = args[i+1];
            args[i] = NULL; 
            args[i + 1] = NULL;
            i++;
        } else{
            args[j++] = args[i];
        }
    }
    args[j] = NULL;
}

int recoger_zombies(int contador_zombies, pid_t pids_zombies[]){
    int i = 0;
    for (i = 0; i < contador_zombies; i++){
        while (waitpid(pids_zombies[i], NULL, WNOHANG) == 0){
            sleep(1);
        }
        printf("Proceso zombie con PID %d eliminado.\n", pids_zombies[i]);
    }
    return 0;
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

    // Creamos un array para almacenar los descriptores de archivo de las tuberías
    int fd[max_commands - 1][2];

    // Creamos las tuberías necesarias (una menos que el número de comandos)
    for (int i = 0; i < num_comandos - 1; i++) {
        if (pipe(fd[i]) < 0) {
            perror("Error al crear la tubería");
            exit(9);
        }
    }

    pid_t pids[max_commands];

    //Finish processing
    for (int i = 0; i < num_comandos; i++) {
        // Se tokeniza el comando para obtener el programa y sus argumentos
        char *args[max_args];
        int args_count = tokenizar_linea(comandos[i], " \t\n", argvv, max_args);
    
        procesar_redirecciones(argvv);

        /********* This piece of code prints the command, args, redirections and background. **********/
        /*********************** REMOVE BEFORE THE SUBMISSION *****************************************/
        /*********************** IMPLEMENT YOUR CODE FOR PROCESSES MANAGEMENT HERE ********************/

        if ((pids[i] = fork()) < 0) {
            perror("Error al crear el fork");
            exit(4);
        }

        if (pids[i] == 0) { // Proceso hijo
            // Conexión de tuberías para entrada (excepto el primer comando)
            if (i > 0) {
                dup2(fd[i - 1][0], STDIN_FILENO); // Bien
            }
            else if (filev[0]) { // Redirección de entrada solo para el primer comando // Bien
                int fd_in = open(filev[0], O_RDONLY);
                if (fd_in < 0) {
                    perror("Error al abrir el archivo de entrada");
                    exit(6);
                }
                dup2(fd_in, STDIN_FILENO);
                close(fd_in);
            }
            
            // Conexión de tuberías para salida (excepto el último comando)
            if (i < num_comandos - 1) {
                dup2(fd[i][1], STDOUT_FILENO); // Bien
            }
            else if (filev[1]) { // Redirección de salida solo para el último comando // Bien
                int fd_out = open(filev[1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if (fd_out < 0) {
                    perror("Error al abrir el archivo de salida");
                    exit(7);
                }
                dup2(fd_out, STDOUT_FILENO);
                close(fd_out);
            }
            
            // Redirección de error (para cualquier comando) // Bien
            if (filev[2]) {
                int fd_err = open(filev[2], O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if (fd_err < 0) {
                    perror("Error al abrir el archivo de error");
                    exit(8);
                }
                dup2(fd_err, STDERR_FILENO);
                close(fd_err);
            }
            
            // Cerramos todos los descriptores de archivo de las tuberías que no usamos
            for (int j = 0; j < num_comandos - 1; j++) {
                close(fd[j][0]);
                close(fd[j][1]);
            }
            execvp(argvv[0], argvv);
            perror("Error en execvp");
            exit(5);
        }
    }
    
    // El proceso padre cierra todos los descriptores de archivo de las tuberías
    for (int i = 0; i < num_comandos - 1; i++) {
        close(fd[i][0]);
        close(fd[i][1]);
    }
    
    // Manejo de background: Mostrar PIDs de todos los hijos si es background
    if (background) {
        for (int i = 0; i < num_comandos; i++) {
            pids_zombies[contador_zombies] = pids[i];
            contador_zombies++;
        }
        printf("Proceso %d en background con PID: %d\n", num_comandos - 1, pids[num_comandos - 1]);
    } else {
        // Si no es background, esperamos a que todos los procesos hijos terminen
        for (int i = 0; i < num_comandos; i++) {
            waitpid(pids[i], NULL, 0);
        }
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
    if (contador_zombies > 0){
        recoger_zombies(contador_zombies, pids_zombies);
        }
    close(fd_entrada);
    return 0;
}