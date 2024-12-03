#include "../include/steganography.h"
#include <stdio.h>
#include <stdlib.h>

int main() {
    const char *bmp_path = "image.bmp"; // Imagem original
    const char *folder_path = "test_folder"; // Pasta com dados a embutir
    const char *output_folder = "extracted_data"; // Pasta onde os dados serão extraídos

    // Embutir os dados da pasta na imagem BMP
    printf("Embutindo dados da pasta '%s' na imagem '%s'...\n", folder_path, bmp_path);
    if (embed_data_in_bmp(bmp_path, folder_path) == 0) {
        printf("Dados embutidos com sucesso! A nova imagem é 'updated_image.bmp'.\n");
    } else {
        printf("Erro ao embutir os dados.\n");
        return 1;
    }

    // Criar a pasta de saída, se não existir
    printf("Criando pasta de saída '%s'...\n", output_folder);
    char command[256];
    snprintf(command, sizeof(command), "mkdir -p %s", output_folder);
    system(command);

    // Extrair os dados da imagem BMP
    printf("Extraindo dados da imagem 'updated_image.bmp' para a pasta '%s'...\n", output_folder);
    if (extract_data_from_bmp("updated_image.bmp", output_folder) == 0) {
        printf("Dados extraídos com sucesso! Verifique a pasta '%s'.\n", output_folder);
    } else {
        printf("Erro ao extrair os dados.\n");
        return 1;
    }

    return 0;
}
