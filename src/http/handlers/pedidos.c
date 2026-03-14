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

void handle_get_pedidos(struct mg_connection *c, struct mg_http_message *hm, dp_db_t db) {
    (void)hm;
    SEND_RESULT(REPO->get_pedidos(db), 200);
}

void handle_get_pedido_by_id(struct mg_connection *c, struct mg_http_message *hm, dp_db_t db) {
    SEND_RESULT(REPO->get_pedido_by_id(db, extract_id_from_uri(hm)), 200);
}

void handle_post_pedidos(struct mg_connection *c, struct mg_http_message *hm, dp_db_t db) {
    char *body = body_to_str(hm);
    if (!body) { send_error_json(c, 400, "BAD_REQUEST", "Body ausente."); return; }
    SEND_RESULT(REPO->save_pedido(db, body), 201);
    free(body);
}

void handle_put_pedido(struct mg_connection *c, struct mg_http_message *hm, dp_db_t db) {
    char *body = body_to_str(hm);
    if (!body) { send_error_json(c, 400, "BAD_REQUEST", "Body ausente."); return; }
    SEND_RESULT(REPO->update_pedido(db, extract_id_from_uri(hm), body), 200);
    free(body);
}

void handle_patch_pedido_status(struct mg_connection *c, struct mg_http_message *hm, dp_db_t db) {
    /* URI: /api/v1/pedidos/{id}/status */
    int id = extract_segment_before_last(hm);
    char *body = body_to_str(hm);
    if (!body) { send_error_json(c, 400, "BAD_REQUEST", "Body ausente."); return; }
    cJSON *j = cJSON_Parse(body);
    free(body);
    if (!j) { send_error_json(c, 400, "BAD_REQUEST", "JSON inválido."); return; }
    const char *status = cJSON_GetStringValue(cJSON_GetObjectItem(j, "status"));
    if (!status) { cJSON_Delete(j); send_error_json(c, 400, "VALIDATION_ERROR", "Campo 'status' obrigatório."); return; }
    SEND_RESULT(REPO->update_pedido_status(db, id, status), 200);
    cJSON_Delete(j);
}

void handle_delete_pedido(struct mg_connection *c, struct mg_http_message *hm, dp_db_t db) {
    SEND_RESULT(REPO->delete_pedido(db, extract_id_from_uri(hm)), 200);
}

void handle_get_pedidos_recentes(struct mg_connection *c, struct mg_http_message *hm, dp_db_t db) {
    (void)hm;
    SEND_RESULT(REPO->get_pedidos_recentes(db), 200);
}
