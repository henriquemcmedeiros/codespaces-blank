#ifndef FUSE_OPERATIONS_H
#define FUSE_OPERATIONS_H

#include <fuse.h>
#include <sys/types.h>
#include <sys/stat.h>

// Estrutura para representar um arquivo ou diretório virtual
typedef struct {
    const char *path;       // Caminho do arquivo
    const char *content;    // Conteúdo do arquivo (NULL para diretórios)
    mode_t mode;            // Tipo e permissões (ex: S_IFREG para arquivo, S_IFDIR para diretório)
    size_t size;            // Tamanho do conteúdo em bytes
} VirtualFile;

// Estrutura de operações do FUSE
extern struct fuse_operations fuse_ops;

// Lista de arquivos virtuais
extern VirtualFile virtual_files[];
extern const size_t num_virtual_files;

#endif // FUSE_OPERATIONS_H
