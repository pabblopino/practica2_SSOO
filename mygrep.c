#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

const int initial_size = 1024;

int main(int argc, char ** argv) {
    if (argc != 3) {
        printf("Usage: %s <ruta_fichero> <cadena_busqueda>\n", argv[0]);
        return -1;
    }

    int fd, n_bytes;
    int linea = 0;
    int found = 0;
    char *buffer = malloc(initial_size);
    char *p = buffer;


    if ((fd = open(argv[1], O_RDONLY)) < 0){
        perror("man 3 perror");
        return -1;
    }

    while ((n_bytes = read(fd, p, 1)) > 0){
        if (*p == '\n' || *p == '\0'){
            *p = '\0'; // Delimitamos el final de la línea en el buffer
            p = buffer; // Volvemos a apuntar p al principio del buffer
            linea ++;
            if (strstr(buffer, argv[2])){
                printf("Cadena encontrada en línea %d\n", linea);
                found = 1;
            }
        }
        else{
            p++; // Incrementamos el puntero byte a byte   
        }
    }

    if (found == 0){
        printf("%s not found.\n", argv[2]);
    }
    
    close(fd);
    return 0;
}

    /*if ((pid = fork()) < 0){
        perror("man 3 perror");
        exit(2);
    }

    if (pid == 0){ // Ejecuta el proceso hijo
        
        exit(0);
    }*/
