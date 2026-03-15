#include "distribpro/http.h"
#include "distribpro/db.h"
#include "distribpro/repo.h"
#include "cJSON.h"
#include <stdlib.h>
#include <string.h>

#define REPO repo_get_instance()

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

/* Declarado em auth.c */
extern int auth_get_current_user(struct mg_http_message *hm);

void handle_get_meus_pedidos(struct mg_connection *c, struct mg_http_message *hm, dp_db_t db) {
    int uid = auth_get_current_user(hm);
    if (uid < 0) { send_error_json(c, 401, "UNAUTHORIZED", "Token invalido ou expirado."); return; }
    SEND_RESULT(REPO->get_meus_pedidos(db, uid), 200);
}

void handle_get_meu_pedido_by_id(struct mg_connection *c, struct mg_http_message *hm, dp_db_t db) {
    int uid = auth_get_current_user(hm);
    if (uid < 0) { send_error_json(c, 401, "UNAUTHORIZED", "Token invalido ou expirado."); return; }
    int pedido_id = extract_id_from_uri(hm);
    SEND_RESULT(REPO->get_meu_pedido_by_id(db, uid, pedido_id), 200);
}

void handle_get_meu_pedido_status(struct mg_connection *c, struct mg_http_message *hm, dp_db_t db) {
    int uid = auth_get_current_user(hm);
    if (uid < 0) { send_error_json(c, 401, "UNAUTHORIZED", "Token invalido ou expirado."); return; }
    /* URI: /api/v1/meus-pedidos/{id}/status */
    int pedido_id = extract_segment_before_last(hm);
    SEND_RESULT(REPO->get_meu_pedido_status(db, uid, pedido_id), 200);
}
