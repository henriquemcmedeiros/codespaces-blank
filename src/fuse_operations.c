#define FUSE_USE_VERSION 31

#include <fuse3/fuse.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <fcntl.h> // Para as permissões de arquivo
#include "fuse_operations.h"

// Lista de arquivos e diretórios virtuais
VirtualFile virtual_files[] = {
    {"/", NULL, S_IFDIR | 0755, 0},         // Diretório raiz
    {"/hello.txt", "Hello, World!", S_IFREG | 0444, 13}, // Arquivo virtual
    {"/example.txt", "Exemplo de conteúdo.", S_IFREG | 0644, 22} // Outro arquivo virtual
};

// Número total de arquivos/diretórios
size_t num_virtual_files = sizeof(virtual_files) / sizeof(VirtualFile);

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
            if (!(virtual_files[i].mode & S_IRUSR)) {  // Verifica permissão de leitura
                return -EACCES; // Permissão negada para leitura
            }
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

int fs_open(const char *path, struct fuse_file_info *fi) {
    // Verifica se o arquivo existe na lista de arquivos virtuais
    for (size_t i = 0; i < num_virtual_files; i++) {
        if (strcmp(path, virtual_files[i].path) == 0) {
            // Verifica permissões de leitura ou escrita
            if ((fi->flags & O_RDONLY) && !(virtual_files[i].mode & S_IRUSR)) {
                return -EACCES; // Permissão de leitura negada
            }
            if ((fi->flags & O_WRONLY) && !(virtual_files[i].mode & S_IWUSR)) {
                return -EACCES; // Permissão de escrita negada
            }
            return 0; // Arquivo aberto com sucesso
        }
    }
    return -ENOENT; // Arquivo não encontrado
}

int fs_create(const char *path, mode_t mode, struct fuse_file_info *fi) {
    // Verifica se o arquivo já existe
    for (size_t i = 0; i < num_virtual_files; i++) {
        if (strcmp(path, virtual_files[i].path) == 0) {
            return -EEXIST; // Arquivo já existe
        }
    }

    // Aloca espaço para um novo arquivo virtual (ajuste para adicionar na lista)
    VirtualFile new_file = {path, NULL, mode | S_IFREG, 0}; // Novo arquivo vazio
    virtual_files[num_virtual_files++] = new_file; // Incrementa num_virtual_files

    return 0; // Arquivo criado com sucesso
}

int fs_unlink(const char *path) {
    // Procura o arquivo e o remove da lista
    for (size_t i = 0; i < num_virtual_files; i++) {
        if (strcmp(path, virtual_files[i].path) == 0) {
            // Desloca os arquivos para remover o arquivo atual
            for (size_t j = i; j < num_virtual_files - 1; j++) {
                virtual_files[j] = virtual_files[j + 1];
            }
            num_virtual_files--; // Atualiza o número de arquivos
            return 0; // Arquivo removido com sucesso
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
    .open = fs_open,
    .create = fs_create,
    .unlink = fs_unlink,
};
