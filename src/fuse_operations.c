#define FUSE_USE_VERSION 31

#include <fuse3/fuse.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>

// Definindo a estrutura VirtualFile
typedef struct {
    const char *path;   // Caminho do arquivo
    const char *content; // Conteúdo do arquivo
    mode_t mode;        // Modo de acesso (por exemplo, se é um arquivo regular ou diretório)
    size_t size;        // Tamanho do arquivo
} VirtualFile;

// Lista de arquivos e diretórios virtuais
VirtualFile virtual_files[] = {
    {"/", NULL, S_IFDIR | 0755, 0},         // Diretório raiz
    {"/hello.txt", "Hello, World!", S_IFREG | 0444, 13}, // Arquivo virtual
    {"/example.txt", "Exemplo de conteúdo.", S_IFREG | 0644, 22} // Outro arquivo virtual
};


// Número total de arquivos/diretórios
const size_t num_virtual_files = sizeof(virtual_files) / sizeof(VirtualFile);

// Função para obter os atributos do arquivo
static int fs_getattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi) {
    (void)fi;
    memset(stbuf, 0, sizeof(struct stat));

    // Itera sobre os arquivos virtuais
    for (size_t i = 0; i < num_virtual_files; i++) {
        if (strcmp(path, virtual_files[i].path) == 0) {
            stbuf->st_mode = virtual_files[i].mode;
            stbuf->st_nlink = (virtual_files[i].mode & S_IFDIR) ? 2 : 1;
            stbuf->st_size = virtual_files[i].size;
            return 0; // Encontrado
        }
    }

    return -ENOENT; // Arquivo não encontrado
}


// Função para listar os diretórios
static int fs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset,
                      struct fuse_file_info *fi, enum fuse_readdir_flags flags) {
    (void)offset;
    (void)fi;
    (void)flags;

    // Certifica-se de que estamos listando o diretório raiz
    if (strcmp(path, "/") != 0)
        return -ENOENT;

    // Diretórios padrão
    filler(buf, ".", NULL, 0, 0);  // Diretório atual
    filler(buf, "..", NULL, 0, 0); // Diretório pai

    // Itera sobre os arquivos virtuais
    for (size_t i = 0; i < num_virtual_files; i++) {
        // Apenas arquivos e diretórios na raiz
        if (strcmp(virtual_files[i].path, "/") != 0) {
            filler(buf, virtual_files[i].path + 1, NULL, 0, 0); // Remove o primeiro '/'
        }
    }

    return 0;
}


// Função para leitura de arquivos
static int fs_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
    (void)fi;

    // Itera sobre os arquivos virtuais
    for (size_t i = 0; i < num_virtual_files; i++) {
        if (strcmp(path, virtual_files[i].path) == 0) {
            if (!virtual_files[i].content)
                return -EISDIR; // Tentativa de leitura em diretório

            size_t len = virtual_files[i].size;
            if (offset >= len)
                return 0; // Fim do arquivo

            if (offset + size > len)
                size = len - offset;

            memcpy(buf, virtual_files[i].content + offset, size);
            return size;
        }
    }

    return -ENOENT; // Arquivo não encontrado
}

// Função para escrita em arquivos
static int fs_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
    (void)fi;

    // Itera sobre os arquivos virtuais
    for (size_t i = 0; i < num_virtual_files; i++) {
        if (strcmp(path, virtual_files[i].path) == 0) {
            if (virtual_files[i].content == NULL) {
                return -EISDIR; // Tentativa de escrita em diretório
            }

            // Certificando que o tamanho do arquivo seja ajustado corretamente
            size_t new_size = offset + size;
            virtual_files[i].size = new_size;
            virtual_files[i].content = realloc((char *)virtual_files[i].content, new_size);
            memcpy((char *)virtual_files[i].content + offset, buf, size);
            return size; // Retorna a quantidade de bytes escritos
        }
    }

    return -ENOENT; // Arquivo não encontrado
}


// Estrutura com as operações do Fuse
struct fuse_operations fuse_ops = {
    .getattr = fs_getattr,
    .readdir = fs_readdir,
    .read = fs_read,
    .write = fs_write, // Adicionando a função de escrita
};
