#ifndef DISTRIBPRO_SUPABASE_STORAGE_H
#define DISTRIBPRO_SUPABASE_STORAGE_H

#include <stddef.h>

/*
 * Faz upload de uma imagem para o Supabase Storage.
 *
 * produto_id  — ID do produto (usado no nome do arquivo)
 * data        — bytes do arquivo
 * data_len    — tamanho em bytes
 * mime_type   — ex.: "image/jpeg", "image/png"
 * out_url     — buffer de saída com a URL pública
 * out_url_len — tamanho do buffer de saída
 *
 * Retorna 0 em sucesso, -1 em caso de erro.
 */
int supabase_upload_image(int produto_id,
                          const char *data, size_t data_len,
                          const char *mime_type,
                          char *out_url, size_t out_url_len);

#endif /* DISTRIBPRO_SUPABASE_STORAGE_H */
