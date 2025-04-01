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
    int buffer_size = initial_size;
    int linea = 0;
    int found = 0;
    char *buffer = malloc(buffer_size);
    if (!buffer) {
        perror("man 3 perror");
        return -1;
    }
    char *p = buffer;


    if ((fd = open(argv[1], O_RDONLY)) < 0){
        perror("man 3 perror");
        return -1;
    }

    while ((n_bytes = read(fd, p, 1)) > 0){
        if (p - buffer >= buffer_size - 1){
            int new_size = buffer_size * 2;
            char *new_buffer = realloc(buffer, new_size);
            if (!new_buffer) {
                perror("man 3 perror");
                free(buffer);
                close(fd);
                return -1;
            }
            p = new_buffer + (p - buffer);
            buffer = new_buffer;
            buffer_size = new_size;
        }
            
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
    
    free(buffer);
    close(fd);
    return 0;
}