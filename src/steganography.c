#include "../include/steganography.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>

// Função para compactar a pasta em um arquivo .tar
int compress_folder(const char *folder_path, const char *output_file) {
    char command[256];
    snprintf(command, sizeof(command), "tar -cf %s -C %s .", output_file, folder_path);
    return system(command);
}

// Função para embutir dados
int embed_data_in_bmp(const char *bmp_path, const char *folder_path) {
    const char *compressed_file = "data.tar";

    if (compress_folder(folder_path, compressed_file) != 0) {
        perror("Erro ao compactar a pasta");
        return -1;
    }

    FILE *bmp_file = fopen(bmp_path, "rb");
    if (!bmp_file) {
        perror("Erro ao abrir o arquivo BMP");
        return -1;
    }

    FILE *output_bmp = fopen("updated_image.bmp", "wb");
    if (!output_bmp) {
        perror("Erro ao criar a nova imagem BMP");
        fclose(bmp_file);
        return -1;
    }

    FILE *data_file = fopen(compressed_file, "rb");
    if (!data_file) {
        perror("Erro ao abrir o arquivo compactado");
        fclose(bmp_file);
        fclose(output_bmp);
        return -1;
    }

    // Copia cabeçalho do BMP
    uint8_t header[54];
    fread(header, 1, 54, bmp_file);
    fwrite(header, 1, 54, output_bmp);

    // Embute os dados no BMP
    uint8_t byte;
    while (fread(&byte, 1, 1, data_file)) {
        uint8_t bmp_byte = fgetc(bmp_file);
        bmp_byte = (bmp_byte & 0xFE) | (byte & 0x01);
        fputc(bmp_byte, output_bmp);
    }

    // Copia o restante do BMP original
    while (fread(&byte, 1, 1, bmp_file)) {
        fputc(byte, output_bmp);
    }

    fclose(data_file);
    fclose(bmp_file);
    fclose(output_bmp);

    return 0;
}

// Função para extrair dados
int extract_data_from_bmp(const char *bmp_path, const char *output_folder) {
    FILE *bmp_file = fopen(bmp_path, "rb");
    if (!bmp_file) {
        perror("Erro ao abrir o arquivo BMP");
        return -1;
    }

    FILE *output_file = fopen("extracted_data.bin", "wb");
    if (!output_file) {
        perror("Erro ao criar o arquivo binário extraído");
        fclose(bmp_file);
        return -1;
    }

    fseek(bmp_file, 54, SEEK_SET); // Pular o cabeçalho da BMP (54 bytes)
    uint8_t byte = 0;
    int bit_pos = 0;

    // Extrair os dados
    while (!feof(bmp_file)) {
        uint8_t bmp_byte = fgetc(bmp_file);
        byte |= ((bmp_byte & 0x01) << bit_pos);

        if (++bit_pos == 8) {
            fputc(byte, output_file);
            byte = 0;
            bit_pos = 0;
        }
    }

    fclose(output_file);
    fclose(bmp_file);

    // Agora, leia os dados binários extraídos
    FILE *bin_file = fopen("extracted_data.bin", "rb");
    if (!bin_file) {
        perror("Erro ao abrir arquivo binário extraído");
        return -1;
    }

    // Crie a pasta de saída se não existir
    char folder_command[256];
    snprintf(folder_command, sizeof(folder_command), "mkdir -p %s", output_folder);
    system(folder_command);

    // Reconstituir os arquivos na pasta de saída
    char file_name[256];
    int file_count = 0;
    while (fread(file_name, sizeof(char), sizeof(file_name), bin_file) == sizeof(file_name)) {
        char file_path[512];
        snprintf(file_path, sizeof(file_path), "%s/%s", output_folder, file_name);

        FILE *extracted_file = fopen(file_path, "wb");
        if (!extracted_file) {
            perror("Erro ao criar o arquivo extraído");
            fclose(bin_file);
            return -1;
        }

        // Copiar o conteúdo do arquivo extraído para o novo arquivo
        uint8_t buffer[1024];
        size_t bytes_read;
        while ((bytes_read = fread(buffer, 1, sizeof(buffer), bin_file)) > 0) {
            fwrite(buffer, 1, bytes_read, extracted_file);
        }

        fclose(extracted_file);
        file_count++;
    }

    fclose(bin_file);
    return 0;
}