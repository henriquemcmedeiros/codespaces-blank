#define FUSE_USE_VERSION 31

#include <fuse3/fuse.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>

// Função para obter os atributos do arquivo
static int fs_getattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi) {
    (void)fi;
    memset(stbuf, 0, sizeof(struct stat));

    if (strcmp(path, "/") == 0) {
        stbuf->st_mode = S_IFDIR | 0755; // Diretório
        stbuf->st_nlink = 2;
    } else if (strcmp(path, "/hello.txt") == 0) {
        stbuf->st_mode = S_IFREG | 0444; // Arquivo somente leitura
        stbuf->st_nlink = 1;
        stbuf->st_size = 12; // Tamanho do arquivo em bytes
    } else {
        return -ENOENT; // Arquivo não encontrado
    }

    return 0;
}

// Função para listar os diretórios
static int fs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset,
                      struct fuse_file_info *fi, enum fuse_readdir_flags flags) {
    (void)offset;
    (void)fi;
    (void)flags;

    if (strcmp(path, "/") != 0)
        return -ENOENT;

    filler(buf, ".", NULL, 0, 0);  // Diretório atual
    filler(buf, "..", NULL, 0, 0); // Diretório pai
    filler(buf, "hello.txt", NULL, 0, 0); // Arquivo virtual

    return 0;
}

// Função para leitura de arquivos
static int fs_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
    (void)fi;

    const char *file_content = "Hello, World!";
    size_t len = strlen(file_content);

    if (strcmp(path, "/hello.txt") != 0)
        return -ENOENT;

    if (offset >= len)
        return 0; // Fim do arquivo

    if (offset + size > len)
        size = len - offset;

    memcpy(buf, file_content + offset, size);

    return size;
}

// Estrutura com as operações do Fuse
struct fuse_operations fuse_ops = {
    .getattr = fs_getattr,
    .readdir = fs_readdir,
    .read = fs_read, // Adicionar a função de leitura
};