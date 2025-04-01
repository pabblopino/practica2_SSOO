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
 * Manejador de señal para SIGCHLD para prevenir procesos zombie
 */
void sigchld_handler(int sig) {
    int status;
    pid_t pid;
    
    // Recolectar todos los procesos hijos terminados sin bloquear
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        if (WIFEXITED(status)) {
            printf("Proceso hijo con PID %d terminado con estado %d\n", pid, WEXITSTATUS(status));
        } else if (WIFSIGNALED(status)) {
            printf("Proceso hijo con PID %d terminado por señal %d\n", pid, WTERMSIG(status));
        }
    }
}

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
 * Retorna un nuevo array de argumentos limpios sin los tokens de redirección.
 */
void procesar_redirecciones(char *args[]) {
    // Inicialización para cada comando
    filev[0] = NULL;
    filev[1] = NULL;
    filev[2] = NULL;
    
    // Limpiar la lista de argumentos
    char *clean_args[max_args];
    for (int i = 0; i < max_args; i++) {
        clean_args[i] = NULL;
    }
    
    int clean_idx = 0;
    
    for (int i = 0; args[i] != NULL; i++) {
        if (strcmp(args[i], "<") == 0) {
            if (args[i+1] != NULL) {
                filev[0] = args[i+1];
                i++; // Saltamos el nombre del archivo
            }
        } else if (strcmp(args[i], ">") == 0) {
            if (args[i+1] != NULL) {
                filev[1] = args[i+1];
                i++; // Saltamos el nombre del archivo
            }
        } else if (strcmp(args[i], "!>") == 0) {
            if (args[i+1] != NULL) {
                filev[2] = args[i+1];
                i++; // Saltamos el nombre del archivo
            }
        } else {
            // Si no es un token de redirección, lo agregamos a la lista limpia
            clean_args[clean_idx++] = args[i];
        }
    }
    
    // Copiamos los argumentos limpios de vuelta a args
    for (int i = 0; i < max_args; i++) {
        args[i] = clean_args[i];
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
    if (num_comandos > 0) {
        char *last_cmd = comandos[num_comandos - 1];
        char *amp_pos = strchr(last_cmd, '&');
        if (amp_pos != NULL) {
            background = 1;
            *amp_pos = '\0';  // Se elimina el caracter '&'
            
            // Si esto deja un espacio al final, lo eliminamos también
            int len = strlen(last_cmd);
            while (len > 0 && (last_cmd[len-1] == ' ' || last_cmd[len-1] == '\t')) {
                last_cmd[len-1] = '\0';
                len--;
            }
        }
    }

    // Creamos un array para almacenar los descriptores de archivo de las tuberías
    int pipes[max_commands - 1][2];
    
    // Creamos las tuberías necesarias
    for (int i = 0; i < num_comandos - 1; i++) {
        if (pipe(pipes[i]) < 0) {
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
        
        // Procesar redirecciones (ahora solo relevantes para el primer y último comando)
        procesar_redirecciones(argvv);

        // Creamos el proceso hijo
        if ((pids[i] = fork()) < 0) {
            perror("Error al crear el fork");
            exit(4);
        }

        if (pids[i] == 0) { // Proceso hijo
            // Conexión de tuberías para entrada (excepto el primer comando)
            if (i > 0) {
                if (dup2(pipes[i - 1][0], STDIN_FILENO) < 0) {
                    perror("Error en dup2 para entrada");
                    exit(10);
                }
            }
            else if (filev[0] != NULL) { // Redirección de entrada solo para el primer comando
                int fd_in = open(filev[0], O_RDONLY);
                if (fd_in < 0) {
                    perror("Error al abrir el archivo de entrada");
                    exit(6);
                }
                if (dup2(fd_in, STDIN_FILENO) < 0) {
                    perror("Error en dup2 para archivo de entrada");
                    exit(11);
                }
                close(fd_in);
            }
            
            // Conexión de tuberías para salida (excepto el último comando)
            if (i < num_comandos - 1) {
                if (dup2(pipes[i][1], STDOUT_FILENO) < 0) {
                    perror("Error en dup2 para salida");
                    exit(12);
                }
            }
            else if (filev[1] != NULL) { // Redirección de salida solo para el último comando
                int fd_out = open(filev[1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if (fd_out < 0) {
                    perror("Error al abrir el archivo de salida");
                    exit(7);
                }
                if (dup2(fd_out, STDOUT_FILENO) < 0) {
                    perror("Error en dup2 para archivo de salida");
                    exit(13);
                }
                close(fd_out);
            }
            
            // Redirección de error (para cualquier comando)
            if (filev[2] != NULL) {
                int fd_err = open(filev[2], O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if (fd_err < 0) {
                    perror("Error al abrir el archivo de error");
                    exit(8);
                }
                if (dup2(fd_err, STDERR_FILENO) < 0) {
                    perror("Error en dup2 para archivo de error");
                    exit(14);
                }
                close(fd_err);
            }
            
            // Cerramos todos los descriptores de archivo de las tuberías que no usamos
            for (int j = 0; j < num_comandos - 1; j++) {
                close(pipes[j][0]);
                close(pipes[j][1]);
            }
            
            // Ejecutar el comando
            execvp(argvv[0], argvv);
            fprintf(stderr, "Error en execvp: %s\n", argvv[0]);
            exit(5);
        }
    }
    
    // El proceso padre cierra todos los descriptores de archivo de las tuberías
    for (int i = 0; i < num_comandos - 1; i++) {
        close(pipes[i][0]);
        close(pipes[i][1]);
    }
    
    // Manejo de background: Mostrar solo el PID del último proceso para comandos en background
    if (background) {
        // Imprimimos solo el PID del último proceso de la secuencia
        if (num_comandos > 0) {
            printf("Proceso en background con PID: %d\n", pids[num_comandos - 1]);
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
        fprintf(stderr, "Formato de ejecución incorrecto: ./scrip <nombre_fichero>\n");
        exit(1);
    }

    // Configurar el manejador de señal SIGCHLD para evitar procesos zombie
    struct sigaction sa;
    sa.sa_handler = sigchld_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("Error al configurar el manejador de señal");
        exit(15);
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
                    fprintf(stderr, "La primera línea es distinta de: ## Script de SSOO\n");
                    exit(3);
                }
                primera_linea = 1;
                p = buffer;
            }
            else if (strlen(buffer) > 0) { // Solo procesamos líneas no vacías
                printf("Ejecutando comando: %s\n", buffer);
                num_comandos = procesar_linea(buffer);
                printf("Número de comandos ejecutados: %d\n\n", num_comandos);
                p = buffer;
            }
            else {
                p = buffer; // Resetear para la siguiente línea
            }
        }
        else {
            p++;
        }
    }
    close(fd_entrada);
    return 0;
}