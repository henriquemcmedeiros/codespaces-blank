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

// Declarações externas para as variáveis
extern VirtualFile virtual_files[];  // Declaração externa para a lista de arquivos
extern const size_t num_virtual_files;  // Declaração externa para o número de arquivos

// Estrutura de operações do FUSE
extern struct fuse_operations fuse_ops;

#endif // FUSE_OPERATIONS_H