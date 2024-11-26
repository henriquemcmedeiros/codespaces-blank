#define FUSE_USE_VERSION 31

#include <fuse.h>
#include <stdio.h>
#include <stdlib.h>
#include "fuse_operations.h"

// Função principal para inicializar o Fuse
int main(int argc, char *argv[]) {
    // Chama a biblioteca FUSE com as operações definidas
    return fuse_main(argc, argv, &fuse_ops, NULL);
}