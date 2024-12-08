#ifndef FUSESTEGANOGRAFIA_H
#define FUSESTEGANOGRAFIA_H

#define FUSE_USE_VERSION 31
#define BMPFS_OPT(t, p) {t, offsetof(struct config_bmpfs, p), 1}

#include "manipuladorBmp.h"
#include <fuse3/fuse.h>
#include <stdint.h>
#include <stdio.h>
#include <stddef.h>
#include <time.h>

struct config_bmpfs {
    char *configuracao_caminho_imagem;
};

#pragma pack(push, 1)
typedef struct {
    char nome_arquivo[256];
    uint64_t tamanho;
    uint64_t criado;
    uint64_t modificado;
    uint64_t acessado;
    uint32_t primeiro_bloco;
    uint32_t num_blocos;
    uint32_t modo;
    uint32_t uid;
    uint32_t gid;
    uint8_t eh_diretorio;
} MetadadosArquivo;
#pragma pack(pop)

_Static_assert(sizeof(MetadadosArquivo) == 309, "MetadadosArquivo deve ter 309 bytes");

typedef struct {
    FILE *arquivo_bmp;
    CabecalhoBMP cabecalho;
    InfoCabecalhoBMP info_cabecalho;
    size_t tamanho_dados;
    size_t tamanho_bloco;
    uint8_t *bitmap;
    MetadadosArquivo *arquivos;
    size_t max_arquivos;
    char *caminho_imagem;
} estado_bmpfs;

extern struct config_bmpfs config_bmpfs;
extern estado_bmpfs estado_sistema_bmpfs;
extern struct fuse_opt opcoes_bmpfs[];
extern struct fuse_operations operacoes_bmpfs;

#endif

