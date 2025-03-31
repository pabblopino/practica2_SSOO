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
#define max_redirections 3 // stdin, stdout, stderr
#define max_args 15

/* VARS TO BE USED FOR THE STUDENTS */
char * argvv[max_args];
char * filev[max_redirections];
int background = 0; 

/**
 * Esta función separa una línea en tokens usando un delimitador.
 * @return Número de tokens.
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
 * Procesa las redirecciones, asignando los archivos correspondientes a filev.
 */
void procesar_redirecciones(char *args[]) {
    // Inicialización para cada comando
    filev[0] = NULL;
    filev[1] = NULL;
    filev[2] = NULL;
    // Se almacena el puntero al nombre del archivo y se limpia el token
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
 * Procesa la línea de comandos y ejecuta los comandos simples o en background.
 */
int procesar_linea(char *linea) {
    background = 0;
    char *comandos[max_commands];
    int num_comandos = tokenizar_linea(linea, "|", comandos, max_commands);

    // Detecta si se debe ejecutar en background buscando el '&'
    if (strchr(comandos[num_comandos - 1], '&')) {
        background = 1;
        char *pos = strchr(comandos[num_comandos - 1], '&');
        *pos = '\0';  // Se elimina el caracter '&'
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
    
    // Procesamos cada comando de la secuencia
    for (int i = 0; i < num_comandos; i++) {
        // Se tokeniza el comando para obtener el programa y sus argumentos
        char *args[max_args];
        int args_count = tokenizar_linea(comandos[i], " \t\n", args, max_args);
        
        // Inicializar argvv con los argumentos tokenizados
        for (int j = 0; j <= args_count; j++) {
            argvv[j] = args[j];
        }
        
        procesar_redirecciones(argvv);

        if ((pids[i] = fork()) < 0) {
            perror("Error al crear el fork");
            exit(4);
        }

        if (pids[i] == 0) { // Proceso hijo
            // Conexión de tuberías para entrada (excepto el primer comando)
            if (i > 0) {
                dup2(fd[i - 1][0], STDIN_FILENO);
            }
            else if (filev[0]) { // Redirección de entrada solo para el primer comando
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
                dup2(fd[i][1], STDOUT_FILENO);
            }
            else if (filev[1]) { // Redirección de salida solo para el último comando
                int fd_out = open(filev[1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if (fd_out < 0) {
                    perror("Error al abrir el archivo de salida");
                    exit(7);
                }
                dup2(fd_out, STDOUT_FILENO);
                close(fd_out);
            }
            
            // Redirección de error (para cualquier comando)
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
            printf("Proceso %d en background con PID: %d\n", i, pids[i]);
        }
    } else {
        // Si no es background, esperamos a que todos los procesos hijos terminen
        for (int i = 0; i < num_comandos; i++) {
            waitpid(pids[i], NULL, 0);
        }
    }
    
    return num_comandos;
}

int main(int argc, char *argv[]) {
    if (argc != 2){
        perror("Formato de ejecución incorrecto: ./scrip <nombre_fichero>");
        exit(1);
    }

    char *fichero_entrada = argv[1];
    char buffer[max_line];
    char *p = buffer;
    int n_bytes, fd_entrada, num_comandos;
    int primera_linea = 0;

    if ((fd_entrada = open(fichero_entrada, O_RDONLY)) < 0){
        perror("Error al abrir el fichero de entrada");
        exit(2);
    } 

    while ((n_bytes = read(fd_entrada, p, 1)) > 0){
        if (*p == '\n' || *p == '\0'){
            *p = '\0';
            if (primera_linea == 0){
                // Verificamos que la primera línea sea la esperada
                if (strcmp(buffer, "## Script de SSOO") != 0){
                    close(fd_entrada);
                    perror("La primera línea es distinta de: ## Script de SSOO");
                    exit(3);
                }
                primera_linea = 1;
                p = buffer;
            }
            else { 
                printf("Ejecutando comando: %s\n", buffer);
                num_comandos = procesar_linea(buffer);
                printf("Número de comandos ejecutados: %d\n\n", num_comandos);
                p = buffer;
            }
        }
        else {
            p++;
        }
    }
    close(fd_entrada);
    return 0;
}