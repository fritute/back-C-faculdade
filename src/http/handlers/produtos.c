#include "distribpro/http.h"
#include "distribpro/db.h"
#include "distribpro/repo.h"
#include "distribpro/supabase_storage.h"
#include <stdlib.h>
#include <string.h>

#define REPO repo_get_instance()

/* Macro para enviar resultado do repo e liberar memória */
#define SEND_RESULT(db_call, success_status) \
    do { \
        DbResult _r = (db_call); \
        if (_r.error) { \
            int _s = strcmp(_r.error,"NOT_FOUND")==0 ? 404 : 500; \
            const char *_c = strcmp(_r.error,"NOT_FOUND")==0 ? "NOT_FOUND" : "DB_ERROR"; \
            send_error_json(c, _s, _c, _r.error); \
            free(_r.error); \
        } else { \
            send_json(c, (success_status), _r.json); \
            free(_r.json); \
        } \
    } while(0)

void handle_get_produtos(struct mg_connection *c, struct mg_http_message *hm, dp_db_t db) {
    (void)hm;
    SEND_RESULT(REPO->get_produtos(db), 200);
}

void handle_get_produto_by_id(struct mg_connection *c, struct mg_http_message *hm, dp_db_t db) {
    SEND_RESULT(REPO->get_produto_by_id(db, extract_id_from_uri(hm)), 200);
}

void handle_post_produtos(struct mg_connection *c, struct mg_http_message *hm, dp_db_t db) {
    char *body = body_to_str(hm);
    if (!body) { send_error_json(c, 400, "BAD_REQUEST", "Body ausente."); return; }
    SEND_RESULT(REPO->save_produto(db, body), 201);
    free(body);
}

void handle_put_produto(struct mg_connection *c, struct mg_http_message *hm, dp_db_t db) {
    char *body = body_to_str(hm);
    if (!body) { send_error_json(c, 400, "BAD_REQUEST", "Body ausente."); return; }
    SEND_RESULT(REPO->update_produto(db, extract_id_from_uri(hm), body), 200);
    free(body);
}

void handle_patch_produto(struct mg_connection *c, struct mg_http_message *hm, dp_db_t db) {
    /* PATCH: mesma lógica de PUT (atualização parcial tratada no cliente) */
    handle_put_produto(c, hm, db);
}

void handle_delete_produto(struct mg_connection *c, struct mg_http_message *hm, dp_db_t db) {
    SEND_RESULT(REPO->delete_produto(db, extract_id_from_uri(hm)), 200);
}

void handle_get_estoque_baixo(struct mg_connection *c, struct mg_http_message *hm, dp_db_t db) {
    (void)hm;
    SEND_RESULT(REPO->get_estoque_baixo(db), 200);
}

/*
 * POST /api/v1/produtos/:id/imagem
 * Content-Type: multipart/form-data; boundary=...
 * Campo do formulário: "imagem" (arquivo binário)
 *
 * Faz upload para o Supabase Storage e salva a URL pública em img_produtos.
 */
void handle_post_produto_imagem(struct mg_connection *c, struct mg_http_message *hm, dp_db_t db) {
    int produto_id = extract_segment_before_last(hm);
    if (produto_id <= 0) {
        send_error_json(c, 400, "BAD_REQUEST", "ID de produto inválido na URL.");
        return;
    }

    /* Extrai o boundary do Content-Type */
    struct mg_str *ct = mg_http_get_header(hm, "Content-Type");
    if (!ct) {
        send_error_json(c, 400, "BAD_REQUEST", "Content-Type ausente.");
        return;
    }

    /* Itera pelas partes do multipart */
    struct mg_http_part part;
    size_t ofs = 0;
    const char *file_data = NULL;
    size_t      file_len  = 0;
    char        mime_type[128] = "image/jpeg";

    while ((ofs = mg_http_next_multipart(hm->body, ofs, &part)) != 0) {
        /* Procura pelo campo "imagem" */
        if (mg_strcasecmp(part.name, mg_str("imagem")) == 0 && part.body.len > 0) {
            file_data = part.body.buf;
            file_len  = part.body.len;
            /* Tenta descobrir o MIME pelo nome do arquivo */
            if (part.filename.len > 4) {
                const char *fn = part.filename.buf;
                size_t fl = part.filename.len;
                if (fl > 4 && strncasecmp(fn + fl - 4, ".png",  4) == 0) snprintf(mime_type, sizeof(mime_type), "image/png");
                else if (fl > 5 && strncasecmp(fn + fl - 5, ".webp", 5) == 0) snprintf(mime_type, sizeof(mime_type), "image/webp");
                else if (fl > 4 && strncasecmp(fn + fl - 4, ".gif",  4) == 0) snprintf(mime_type, sizeof(mime_type), "image/gif");
                else snprintf(mime_type, sizeof(mime_type), "image/jpeg");
            }
            break;
        }
    }

    if (!file_data || file_len == 0) {
        send_error_json(c, 400, "BAD_REQUEST", "Campo 'imagem' ausente ou vazio no formulário.");
        return;
    }

    /* Limite de tamanho: 5 MB */
    if (file_len > 5 * 1024 * 1024) {
        send_error_json(c, 413, "FILE_TOO_LARGE", "Imagem excede o limite de 5 MB.");
        return;
    }

    /* Faz upload para o Supabase Storage */
    char public_url[512] = {0};
    if (supabase_upload_image(produto_id, file_data, file_len, mime_type,
                              public_url, sizeof(public_url)) != 0) {
        send_error_json(c, 502, "UPLOAD_ERROR", "Falha ao enviar imagem para o Supabase Storage.");
        return;
    }

    /* Persiste a URL no banco */
    SEND_RESULT(REPO->update_produto_imagem(db, produto_id, public_url), 200);
}
