#define FUSE_USE_VERSION 31

#include <fuse.h>
#include <stdio.h>
#include <stdlib.h>
#include "fuse_operations.h"

// Função principal para inicializar o sistema de arquivos FUSE
int main(int argc, char *argv[]) {
    // Verifica se o número de argumentos está correto
    if (argc < 2) {
        fprintf(stderr, "Uso: %s <ponto_de_montagem>\n", argv[0]);
        return EXIT_FAILURE;
    }

    // Chama a função fuse_main para montar o sistema de arquivos com as operações definidas
    return fuse_main(argc, argv, &fuse_ops, NULL);
}