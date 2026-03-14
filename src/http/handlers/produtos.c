#include "distribpro/http.h"
#include "distribpro/db.h"
#include "distribpro/repo.h"
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
