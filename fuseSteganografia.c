#include "fuseSteganografia.h"
#include "manipuladorBmp.h"
#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdarg.h>
#include <time.h>
#include <limits.h>

struct config_bmpfs config_bmpfs;
estado_bmpfs estado_sistema_bmpfs;

struct fuse_opt opcoes_bmpfs[] = {
    BMPFS_OPT("imagem=%s", configuracao_caminho_imagem),
    FUSE_OPT_END
};

static size_t calcular_tamanho_metadados(estado_bmpfs *estado) {
    size_t total_blocos = estado->tamanho_dados / estado->tamanho_bloco;
    size_t tamanho_bitmap = total_blocos;
    size_t tamanho_metadados_arquivo = estado->max_arquivos * sizeof(MetadadosArquivo);
    return tamanho_bitmap + tamanho_metadados_arquivo;
}

static int ler_metadados(estado_bmpfs *estado) {
    size_t tamanho_metadados = calcular_tamanho_metadados(estado);
    char *buffer = malloc(tamanho_metadados);

    if (!buffer) {
        return -ENOMEM;
    }

    if (fseek(estado->arquivo_bmp, estado->cabecalho.deslocamento_dados, SEEK_SET) != 0) {
        free(buffer);
        return -EIO;
    }

    size_t bytes_lidos = fread(buffer, 1, tamanho_metadados, estado->arquivo_bmp);
    if (bytes_lidos != tamanho_metadados) {
        free(buffer);
        return -EIO;
    }

    size_t tamanho_bitmap = estado->tamanho_dados / estado->tamanho_bloco;
    memcpy(estado->bitmap, buffer, tamanho_bitmap);
    memcpy(estado->arquivos, buffer + tamanho_bitmap, estado->max_arquivos * sizeof(MetadadosArquivo));
    free(buffer);
    return 0;
}

static int escrever_metadados(estado_bmpfs *estado) {
    size_t tamanho_metadados = calcular_tamanho_metadados(estado);
    char *buffer = malloc(tamanho_metadados);

    if (!buffer) {
        return -ENOMEM;
    }

    size_t tamanho_bitmap = estado->tamanho_dados / estado->tamanho_bloco;
    memcpy(buffer, estado->bitmap, tamanho_bitmap);
    memcpy(buffer + tamanho_bitmap, estado->arquivos, estado->max_arquivos * sizeof(MetadadosArquivo));

    if (fseek(estado->arquivo_bmp, estado->cabecalho.deslocamento_dados, SEEK_SET) != 0) {
        free(buffer);
        return -EIO;
    }

    size_t bytes_escritos = fwrite(buffer, 1, tamanho_metadados, estado->arquivo_bmp);

    if (bytes_escritos != tamanho_metadados) {
        free(buffer);
        return -EIO;
    }

    if (fflush(estado->arquivo_bmp) != 0) {
        free(buffer);
        return -EIO;
    }

    free(buffer);
    return 0;
}

static int validar_caminho(const char *caminho) {
    if (!caminho || strlen(caminho) >= 256) {
        return -ENAMETOOLONG;
    }

    if (caminho[0] == '/') {
        caminho++;
    }

    if (strchr(caminho, '/')) {
        return -EINVAL;
    }

    return 0;
}

static int caminho_para_indice_metadados(const char *caminho) {
    int validacao = validar_caminho(caminho);
    if (validacao < 0) {
        return validacao;
    }

    const char *nome = caminho;

    if (caminho[0] == '/') {
        nome++;
    }

    for (size_t i = 0; i < estado_sistema_bmpfs.max_arquivos; i++) {
        if (estado_sistema_bmpfs.arquivos[i].nome_arquivo[0] != '\0' && strcmp(estado_sistema_bmpfs.arquivos[i].nome_arquivo, nome) == 0) {
            return i;
        }
    }

    return -ENOENT;
}

static uint32_t encontrar_blocos_livres(size_t num_blocos) {
    if (num_blocos == 0) {
        return 0;
    }

    size_t total_blocos = estado_sistema_bmpfs.tamanho_dados / estado_sistema_bmpfs.tamanho_bloco;
    size_t consecutivos = 0;
    uint32_t bloco_inicio = 0;

    for (size_t i = 0; i < total_blocos; i++) {
        if (estado_sistema_bmpfs.bitmap[i] == 0) {
            if (consecutivos == 0) {
                bloco_inicio = i;
            }
            consecutivos++;

            if (consecutivos >= num_blocos) {
                return bloco_inicio;
            }
        } else {
            consecutivos = 0;
        }
    }

    return UINT32_MAX;
}

static int ler_blocos(uint32_t bloco_inicio, size_t num_blocos, char *buffer) {
    if (!buffer || !estado_sistema_bmpfs.arquivo_bmp) {
        return -EINVAL;
    }

    size_t tamanho_metadados = calcular_tamanho_metadados(&estado_sistema_bmpfs);
    size_t offset = estado_sistema_bmpfs.cabecalho.deslocamento_dados + tamanho_metadados + (bloco_inicio * estado_sistema_bmpfs.tamanho_bloco);

    if (fseek(estado_sistema_bmpfs.arquivo_bmp, offset, SEEK_SET) != 0) {
        return -EIO;
    }

    size_t bytes_lidos = fread(buffer, 1, estado_sistema_bmpfs.tamanho_bloco * num_blocos, estado_sistema_bmpfs.arquivo_bmp);

    if (bytes_lidos != estado_sistema_bmpfs.tamanho_bloco * num_blocos) {
        return -EIO;
    }
    return 0;
}

static int escrever_blocos(uint32_t bloco_inicio, size_t num_blocos, const char *buffer) {
    if (!buffer || !estado_sistema_bmpfs.arquivo_bmp) {
        return -EINVAL;
    }

    size_t tamanho_metadados = calcular_tamanho_metadados(&estado_sistema_bmpfs);

    size_t offset = estado_sistema_bmpfs.cabecalho.deslocamento_dados + tamanho_metadados + (bloco_inicio * estado_sistema_bmpfs.tamanho_bloco);

    if (fseek(estado_sistema_bmpfs.arquivo_bmp, offset, SEEK_SET) != 0) {
        return -EIO;
    }

    size_t bytes_escritos = fwrite(buffer, 1, estado_sistema_bmpfs.tamanho_bloco * num_blocos, estado_sistema_bmpfs.arquivo_bmp);

    if (bytes_escritos != estado_sistema_bmpfs.tamanho_bloco * num_blocos) {
        return -EIO;
    }

    if (fflush(estado_sistema_bmpfs.arquivo_bmp) != 0) {
        return -EIO;
    }

    return 0;
}

static int getattr_bmpfs(const char *caminho, struct stat *stbuf, struct fuse_file_info *fi) {
    (void) fi;
    memset(stbuf, 0, sizeof(struct stat));

    if (strcmp(caminho, "/") == 0) {
        stbuf->st_mode = S_IFDIR | 0755;
        stbuf->st_nlink = 2;
        stbuf->st_uid = getuid();
        stbuf->st_gid = getgid();
        stbuf->st_atime = time(NULL);
        stbuf->st_mtime = stbuf->st_atime;
        stbuf->st_ctime = stbuf->st_atime;
        return 0;
    }

    int idx = caminho_para_indice_metadados(caminho);

    if (idx < 0) {
        return idx;
    }

    MetadadosArquivo *meta = &estado_sistema_bmpfs.arquivos[idx];
    stbuf->st_mode = meta->modo;
    stbuf->st_nlink = meta->eh_diretorio ? 2 : 1;
    stbuf->st_size = meta->tamanho;
    stbuf->st_uid = meta->uid;
    stbuf->st_gid = meta->gid;
    stbuf->st_atime = meta->acessado;
    stbuf->st_mtime = meta->modificado;
    stbuf->st_ctime = meta->criado;
    stbuf->st_blocks = (meta->tamanho + 511) / 512;
    stbuf->st_blksize = estado_sistema_bmpfs.tamanho_bloco;

    return 0;
}

static int criar_diretorio(const char *caminho, mode_t modo) {
    int validacao = validar_caminho(caminho);

    if (validacao < 0) {
        return validacao;
    }

    if (caminho_para_indice_metadados(caminho) >= 0) {
        return -EEXIST;
    }

    int idx = -1;

    for (size_t i = 0; i < estado_sistema_bmpfs.max_arquivos; i++) {
        if (estado_sistema_bmpfs.arquivos[i].nome_arquivo[0] == '\0') {
            idx = i;
            break;
        }
    }

    if (idx < 0) {
        return -ENOMEM;
    }

    MetadadosArquivo *meta = &estado_sistema_bmpfs.arquivos[idx];

    const char *nome_diretorio = caminho;

    if (caminho[0] == '/') {
        nome_diretorio++;
    }

    strncpy(meta->nome_arquivo, nome_diretorio, sizeof(meta->nome_arquivo) - 1);
    meta->nome_arquivo[sizeof(meta->nome_arquivo) - 1] = '\0';
    meta->tamanho = 0;
    meta->criado = time(NULL);
    meta->modificado = meta->criado;
    meta->acessado = meta->criado;
    meta->primeiro_bloco = UINT32_MAX;
    meta->num_blocos = 0;
    meta->modo = S_IFDIR | (modo & 0777);
    meta->uid = getuid();
    meta->gid = getgid();
    meta->eh_diretorio = 1;

    if (escrever_metadados(&estado_sistema_bmpfs) < 0) {
        return -EIO;
    }

    return 0;
}

static int criar_bmpfs(const char *caminho, mode_t modo, struct fuse_file_info *fi) {
    (void) fi;

    int validacao = validar_caminho(caminho);

    if (validacao < 0) {
        return validacao;
    }

    if (caminho_para_indice_metadados(caminho) >= 0) {
        return -EEXIST;
    }
    
    int idx = -1;

    for (size_t i = 0; i < estado_sistema_bmpfs.max_arquivos; i++) {
        if (estado_sistema_bmpfs.arquivos[i].nome_arquivo[0] == '\0') {
            idx = i;
            break;
        }
    }

    if (idx < 0) {
        return -ENOMEM;
    }

    MetadadosArquivo *meta = &estado_sistema_bmpfs.arquivos[idx];
    const char *nome_arquivo = caminho;

    if (caminho[0] == '/') {
        nome_arquivo++;
    }

    strncpy(meta->nome_arquivo, nome_arquivo, sizeof(meta->nome_arquivo) - 1);
    meta->nome_arquivo[sizeof(meta->nome_arquivo) - 1] = '\0';
    meta->tamanho = 0;
    meta->criado = time(NULL);
    meta->modificado = meta->criado;
    meta->acessado = meta->criado;
    meta->primeiro_bloco = UINT32_MAX;
    meta->num_blocos = 0;
    meta->modo = S_IFREG | (modo & 0777);
    meta->uid = getuid();
    meta->gid = getgid();
    meta->eh_diretorio = 0;

    if (escrever_metadados(&estado_sistema_bmpfs) < 0) {
        return -EIO;
    }

    return 0;
}

static int excluir_bmpfs(const char *caminho) {
    int idx = caminho_para_indice_metadados(caminho);

    if (idx < 0) {
        return idx;
    }

    MetadadosArquivo *meta = &estado_sistema_bmpfs.arquivos[idx];
    if (meta->eh_diretorio) {
        return -EISDIR;
    }

    for (uint32_t i = 0; i < meta->num_blocos; i++) {
        estado_sistema_bmpfs.bitmap[meta->primeiro_bloco + i] = 0;
    }

    memset(meta, 0, sizeof(MetadadosArquivo));

    if (escrever_metadados(&estado_sistema_bmpfs) < 0) {
        return -EIO;
    }

    return 0;
}

static int ler_bmpfs(const char *caminho, char *buf, size_t tamanho, off_t offset, struct fuse_file_info *fi) {
    (void) fi;

    if (!buf) {
        return -EINVAL;
    }

    int idx = caminho_para_indice_metadados(caminho);

    if (idx < 0) {
        return idx;
    }

    MetadadosArquivo *meta = &estado_sistema_bmpfs.arquivos[idx];

    if (meta->eh_diretorio) {
        return -EISDIR;
    }

    meta->acessado = time(NULL);

    if (offset < 0) {
        return -EINVAL;
    }

    if ((uint64_t)offset >= meta->tamanho) {
        return 0;
    }

    if ((uint64_t)(offset + tamanho) > meta->tamanho) {
        tamanho = meta->tamanho - offset;
    }

    uint32_t bloco_inicio = meta->primeiro_bloco + (offset / estado_sistema_bmpfs.tamanho_bloco);
    size_t deslocamento_bloco = offset % estado_sistema_bmpfs.tamanho_bloco;
    size_t blocos_para_ler = (tamanho + deslocamento_bloco + estado_sistema_bmpfs.tamanho_bloco - 1) / estado_sistema_bmpfs.tamanho_bloco;
    char *buffer_temp = malloc(blocos_para_ler * estado_sistema_bmpfs.tamanho_bloco);

    if (!buffer_temp) {
        return -ENOMEM;
    }

    int resultado_leitura = ler_blocos(bloco_inicio, blocos_para_ler, buffer_temp);

    if (resultado_leitura < 0) {
        free(buffer_temp);
        return resultado_leitura;
    }

    memcpy(buf, buffer_temp + deslocamento_bloco, tamanho);
    free(buffer_temp);

    return (int)tamanho;
}

static int escrever_bmpfs(const char *caminho, const char *buf, size_t tamanho, off_t offset, struct fuse_file_info *fi) {
    (void) fi;

    if (!buf) {
        return -EINVAL;
    }

    int idx = caminho_para_indice_metadados(caminho);

    if (idx < 0) {
        return idx;
    }

    MetadadosArquivo *meta = &estado_sistema_bmpfs.arquivos[idx];

    if (meta->eh_diretorio) {
        return -EISDIR;
    }

    if (offset < 0) {
        return -EINVAL;
    }

    size_t novo_tamanho = (size_t)offset + tamanho;

    if (novo_tamanho < (size_t)offset) {
        return -EFBIG;
    }

    size_t novos_blocos = (novo_tamanho + estado_sistema_bmpfs.tamanho_bloco - 1) / estado_sistema_bmpfs.tamanho_bloco;

    if (novos_blocos > meta->num_blocos) {
        uint32_t novo_inicio = encontrar_blocos_livres(novos_blocos);

        if (novo_inicio == UINT32_MAX) {
            return -ENOSPC;
        }

        if (meta->num_blocos > 0) {
            char *buffer_temp = malloc(meta->num_blocos * estado_sistema_bmpfs.tamanho_bloco);

            if (!buffer_temp) {
                return -ENOMEM;
            }

            int resultado_leitura = ler_blocos(meta->primeiro_bloco, meta->num_blocos, buffer_temp);

            if (resultado_leitura < 0) {
                free(buffer_temp);
                return resultado_leitura;
            }

            int resultado_escrita = escrever_blocos(novo_inicio, meta->num_blocos, buffer_temp);
            free(buffer_temp);

            if (resultado_escrita < 0) {
                return resultado_escrita;
            }

            for (uint32_t i = 0; i < meta->num_blocos; i++) {
                estado_sistema_bmpfs.bitmap[meta->primeiro_bloco + i] = 0;
            }
        }

        meta->primeiro_bloco = novo_inicio;

        for (uint32_t i = 0; i < novos_blocos; i++) {
            estado_sistema_bmpfs.bitmap[novo_inicio + i] = 1;
        }

        meta->num_blocos = novos_blocos;
    }

    uint32_t bloco_inicio = meta->primeiro_bloco + (offset / estado_sistema_bmpfs.tamanho_bloco);
    size_t deslocamento_bloco = offset % estado_sistema_bmpfs.tamanho_bloco;
    size_t blocos_para_escrever = (tamanho + deslocamento_bloco + estado_sistema_bmpfs.tamanho_bloco - 1) / estado_sistema_bmpfs.tamanho_bloco;
    char *buffer_temp = malloc(blocos_para_escrever * estado_sistema_bmpfs.tamanho_bloco);

    if (!buffer_temp) {
        return -ENOMEM;
    }

    if (deslocamento_bloco > 0 || (tamanho % estado_sistema_bmpfs.tamanho_bloco) != 0) {
        int resultado_leitura = ler_blocos(bloco_inicio, blocos_para_escrever, buffer_temp);

        if (resultado_leitura < 0) {
            free(buffer_temp);
            return resultado_leitura;
        }
    } else {
        memset(buffer_temp, 0, blocos_para_escrever * estado_sistema_bmpfs.tamanho_bloco);
    }

    memcpy(buffer_temp + deslocamento_bloco, buf, tamanho);
    int resultado_escrita = escrever_blocos(bloco_inicio, blocos_para_escrever, buffer_temp);
    free(buffer_temp);

    if (resultado_escrita < 0) {
        return resultado_escrita;
    }

    if (novo_tamanho > meta->tamanho) {
        meta->tamanho = novo_tamanho;
    }

    meta->modificado = time(NULL);

    if (escrever_metadados(&estado_sistema_bmpfs) < 0) {
        return -EIO;
    }

    return (int)tamanho;
}

static int readdir_bmpfs(const char *caminho, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi, enum fuse_readdir_flags flags) {
    (void) offset;
    (void) fi;
    (void) flags;

    if (strcmp(caminho, "/") != 0) {
        return -ENOENT;
    }

    if (filler(buf, ".", NULL, 0, 0) || filler(buf, "..", NULL, 0, 0)) {
        return -ENOMEM;
    }

    for (size_t i = 0; i < estado_sistema_bmpfs.max_arquivos; i++) {
        if (estado_sistema_bmpfs.arquivos[i].nome_arquivo[0] != '\0') {
            struct stat st;
            memset(&st, 0, sizeof(struct stat));
            st.st_mode = estado_sistema_bmpfs.arquivos[i].modo;
            st.st_nlink = estado_sistema_bmpfs.arquivos[i].eh_diretorio ? 2 : 1;
            st.st_size = estado_sistema_bmpfs.arquivos[i].tamanho;
            st.st_uid = estado_sistema_bmpfs.arquivos[i].uid;
            st.st_gid = estado_sistema_bmpfs.arquivos[i].gid;
            st.st_atime = estado_sistema_bmpfs.arquivos[i].acessado;
            st.st_mtime = estado_sistema_bmpfs.arquivos[i].modificado;
            st.st_ctime = estado_sistema_bmpfs.arquivos[i].criado;
            st.st_blocks = (estado_sistema_bmpfs.arquivos[i].tamanho + 511) / 512;
            st.st_blksize = estado_sistema_bmpfs.tamanho_bloco;

            if (estado_sistema_bmpfs.arquivos[i].eh_diretorio) {
                st.st_mode |= S_IFDIR;
            } else {
                st.st_mode |= S_IFREG;
            }

            if (filler(buf, estado_sistema_bmpfs.arquivos[i].nome_arquivo, &st, 0, 0)) {
                return -ENOMEM;
            }
        }
    }

    return 0;
}

static int truncar_bmpfs(const char *caminho, off_t tamanho, struct fuse_file_info *fi) {
    (void) fi;

    if (tamanho < 0) {
        return -EINVAL;
    }

    int idx = caminho_para_indice_metadados(caminho);

    if (idx < 0) {
        return idx;
    }

    MetadadosArquivo *meta = &estado_sistema_bmpfs.arquivos[idx];

    if (meta->eh_diretorio) {
        return -EISDIR;
    }

    size_t novos_blocos = (tamanho + estado_sistema_bmpfs.tamanho_bloco - 1) / estado_sistema_bmpfs.tamanho_bloco;

    if (tamanho == 0) {
        for (uint32_t i = 0; i < meta->num_blocos; i++) {
            estado_sistema_bmpfs.bitmap[meta->primeiro_bloco + i] = 0;
        }
        meta->primeiro_bloco = UINT32_MAX;
        meta->num_blocos = 0;
        meta->tamanho = 0;
        meta->modificado = time(NULL);
    } else if (novos_blocos < meta->num_blocos) {
        for (uint32_t i = novos_blocos; i < meta->num_blocos; i++) {
            estado_sistema_bmpfs.bitmap[meta->primeiro_bloco + i] = 0;
        }

        meta->num_blocos = novos_blocos;
        meta->tamanho = tamanho;
        meta->modificado = time(NULL);
    } else if (novos_blocos > meta->num_blocos) {
        uint32_t novo_inicio = encontrar_blocos_livres(novos_blocos);

        if (novo_inicio == UINT32_MAX) {
            return -ENOSPC;
        }
        if (meta->num_blocos > 0) {
            char *buffer_temp = malloc(meta->num_blocos * estado_sistema_bmpfs.tamanho_bloco);
            if (!buffer_temp) {
                return -ENOMEM;
            }
            int resultado_leitura = ler_blocos(meta->primeiro_bloco, meta->num_blocos, buffer_temp);
            if (resultado_leitura < 0) {
                free(buffer_temp);
                return resultado_leitura;
            }
            int resultado_escrita = escrever_blocos(novo_inicio, meta->num_blocos, buffer_temp);
            free(buffer_temp);
            if (resultado_escrita < 0) {
                return resultado_escrita;
            }
            for (uint32_t i = 0; i < meta->num_blocos; i++) {
                estado_sistema_bmpfs.bitmap[meta->primeiro_bloco + i] = 0;
            }
        }

        for (uint32_t i = 0; i < novos_blocos; i++) {
            estado_sistema_bmpfs.bitmap[novo_inicio + i] = 1;
        }
        meta->primeiro_bloco = novo_inicio;
        meta->num_blocos = novos_blocos;
        meta->tamanho = tamanho;
        meta->modificado = time(NULL);
    }

    if (escrever_metadados(&estado_sistema_bmpfs) < 0) {
        return -EIO;
    }

    return 0;
}

static int atualizar_tempo_bmpfs(const char *caminho, const struct timespec ts[2], struct fuse_file_info *fi) {
    (void) fi;
    int idx = caminho_para_indice_metadados(caminho);

    if (idx < 0) {
        return idx;
    }

    MetadadosArquivo *meta = &estado_sistema_bmpfs.arquivos[idx];

    if (ts) {
        meta->acessado = ts[0].tv_sec;
        meta->modificado = ts[1].tv_sec;
    } else {
        time_t atual = time(NULL);
        meta->acessado = atual;
        meta->modificado = atual;
    }
    return 0;
}

static int fsync_bmpfs(const char *caminho, int datasync, struct fuse_file_info *fi) {
    (void) caminho;
    (void) fi;

    if (!estado_sistema_bmpfs.arquivo_bmp) {
        return -EIO;
    }

    if (datasync) {
        return fdatasync(fileno(estado_sistema_bmpfs.arquivo_bmp));
    } else {
        return fsync(fileno(estado_sistema_bmpfs.arquivo_bmp));
    }
}

static int abrir_bmpfs(const char *caminho, struct fuse_file_info *fi) {
    int idx = caminho_para_indice_metadados(caminho);

    if (idx < 0) {
        return idx;
    }
    MetadadosArquivo *meta = &estado_sistema_bmpfs.arquivos[idx];

    if (meta->eh_diretorio && (fi->flags & O_WRONLY)) {
        return -EACCES;
    }

    if ((fi->flags & O_WRONLY) && !(meta->modo & S_IWUSR)) {
        return -EACCES;
    }

    if ((fi->flags & O_RDONLY) && !(meta->modo & S_IRUSR)) {
        return -EACCES;
    }
    meta->acessado = time(NULL);

    return 0;
}

static int remover_diretorio_bmpfs(const char *caminho) {
    int idx = caminho_para_indice_metadados(caminho);

    if (idx < 0) {
        return idx;
    }

    MetadadosArquivo *meta = &estado_sistema_bmpfs.arquivos[idx];

    if (!meta->eh_diretorio) {
        return -ENOTDIR;
    }

    for (uint32_t i = 0; i < meta->num_blocos; i++) {
        estado_sistema_bmpfs.bitmap[meta->primeiro_bloco + i] = 0;
    }

    memset(meta, 0, sizeof(MetadadosArquivo));
    if (escrever_metadados(&estado_sistema_bmpfs) < 0) {
        return -EIO;
    }

    return 0;
}

static void *inicializar_bmpfs(struct fuse_conn_info *conn, struct fuse_config *cfg) {
    (void) conn;
    cfg->kernel_cache = 1;
    cfg->entry_timeout = 60.0;
    cfg->attr_timeout = 60.0;

    if (!estado_sistema_bmpfs.caminho_imagem) {
        return NULL;
    }

    estado_sistema_bmpfs.arquivo_bmp = fopen(estado_sistema_bmpfs.caminho_imagem, "r+b");

    if (!estado_sistema_bmpfs.arquivo_bmp) {
        int resultado_criacao = criar_arquivo_bmp(estado_sistema_bmpfs.caminho_imagem, 2048, 2048);
        if (resultado_criacao < 0) {
            return NULL;
        }

        estado_sistema_bmpfs.arquivo_bmp = fopen(estado_sistema_bmpfs.caminho_imagem, "r+b");
        if (!estado_sistema_bmpfs.arquivo_bmp) {
            return NULL;
        }
    }

    int fd = fileno(estado_sistema_bmpfs.arquivo_bmp);

    if (fd == -1) {
        fclose(estado_sistema_bmpfs.arquivo_bmp);
        return NULL;
    }

    struct stat st;

    if (fstat(fd, &st) == -1) {
        fclose(estado_sistema_bmpfs.arquivo_bmp);
        return NULL;
    }

    if ((st.st_mode & S_IRUSR) == 0 || (st.st_mode & S_IWUSR) == 0) {
        fclose(estado_sistema_bmpfs.arquivo_bmp);
        return NULL;
    }

    CabecalhoBMP cabecalho;
    InfoCabecalhoBMP info_cabecalho;

    if (ler_cabecalho_bmp(estado_sistema_bmpfs.arquivo_bmp, &cabecalho, &info_cabecalho) < 0) {
        fclose(estado_sistema_bmpfs.arquivo_bmp);
        return NULL;
    }

    estado_sistema_bmpfs.cabecalho = cabecalho;
    estado_sistema_bmpfs.info_cabecalho = info_cabecalho;
    size_t tamanho_linha = (info_cabecalho.largura * 3 + 3) & ~3;
    estado_sistema_bmpfs.tamanho_dados = tamanho_linha * info_cabecalho.altura;
    estado_sistema_bmpfs.tamanho_bloco = 512;
    estado_sistema_bmpfs.max_arquivos = 1000;
    size_t tamanho_bitmap = estado_sistema_bmpfs.tamanho_dados / estado_sistema_bmpfs.tamanho_bloco;
    estado_sistema_bmpfs.bitmap = calloc(tamanho_bitmap, sizeof(uint8_t));

    if (!estado_sistema_bmpfs.bitmap) {
        fclose(estado_sistema_bmpfs.arquivo_bmp);
        return NULL;
    }

    estado_sistema_bmpfs.arquivos = calloc(estado_sistema_bmpfs.max_arquivos, sizeof(MetadadosArquivo));
    if (!estado_sistema_bmpfs.arquivos) {
        free(estado_sistema_bmpfs.bitmap);
        fclose(estado_sistema_bmpfs.arquivo_bmp);
        return NULL;
    }

    if (ler_metadados(&estado_sistema_bmpfs) < 0) {
        free(estado_sistema_bmpfs.bitmap);
        free(estado_sistema_bmpfs.arquivos);
        fclose(estado_sistema_bmpfs.arquivo_bmp);
        return NULL;
    }

    return &estado_sistema_bmpfs;
}

static void destruir_bmpfs(void *dados_privados) {
    (void) dados_privados;

    if (escrever_metadados(&estado_sistema_bmpfs) < 0) {
    }

    if (estado_sistema_bmpfs.arquivo_bmp) {
        fclose(estado_sistema_bmpfs.arquivo_bmp);
        estado_sistema_bmpfs.arquivo_bmp = NULL;
    }

    free(estado_sistema_bmpfs.bitmap);
    estado_sistema_bmpfs.bitmap = NULL;
    free(estado_sistema_bmpfs.arquivos);
    estado_sistema_bmpfs.arquivos = NULL;
    free(estado_sistema_bmpfs.caminho_imagem);
    estado_sistema_bmpfs.caminho_imagem = NULL;
}

struct fuse_operations operacoes_bmpfs = {
    .init       = inicializar_bmpfs,
    .destroy    = destruir_bmpfs,
    .getattr    = getattr_bmpfs,
    .readdir    = readdir_bmpfs,
    .create     = criar_bmpfs,
    .unlink     = excluir_bmpfs,
    .read       = ler_bmpfs,
    .write      = escrever_bmpfs,
    .open       = abrir_bmpfs,
    .truncate   = truncar_bmpfs,
    .utimens    = atualizar_tempo_bmpfs,
    .fsync      = fsync_bmpfs,
    .mkdir      = criar_diretorio,
    .rmdir      = remover_diretorio_bmpfs,
};
