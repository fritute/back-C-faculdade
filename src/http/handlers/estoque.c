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

void handle_get_estoque(struct mg_connection *c, struct mg_http_message *hm, dp_db_t db) {
    (void)hm;
    SEND_RESULT(REPO->get_estoque(db), 200);
}

void handle_patch_estoque(struct mg_connection *c, struct mg_http_message *hm, dp_db_t db) {
    int produto_id = extract_id_from_uri(hm);
    char *body = body_to_str(hm);
    if (!body) { send_error_json(c, 400, "BAD_REQUEST", "Body ausente."); return; }
    cJSON *j = cJSON_Parse(body);
    free(body);
    if (!j) { send_error_json(c, 400, "BAD_REQUEST", "JSON inválido."); return; }
    cJSON *qtd = cJSON_GetObjectItem(j, "estoque");
    if (!qtd) qtd = cJSON_GetObjectItem(j, "qtd");
    if (!qtd) { cJSON_Delete(j); send_error_json(c, 400, "VALIDATION_ERROR", "Campo 'estoque' obrigatório."); return; }
    int novo_estoque = (int)qtd->valuedouble;
    cJSON_Delete(j);
    SEND_RESULT(REPO->ajustar_estoque(db, produto_id, novo_estoque), 200);
}
