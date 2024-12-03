#ifndef STEGANOGRAPHY_H
#define STEGANOGRAPHY_H

#include <stdint.h>

// Insere dados da pasta na imagem BMP
int embed_data_in_bmp(const char *bmp_path, const char *folder_path);

// Extrai dados de uma imagem BMP para a pasta
int extract_data_from_bmp(const char *bmp_path, const char *output_folder);

// Comprime os dados da pasta
int compress_folder(const char *folder_path, const char *output_file);

#endif // STEGANOGRAPHY_H
