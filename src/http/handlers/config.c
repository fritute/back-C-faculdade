#include "distribpro/http.h"
#include "distribpro/db.h"
#include "distribpro/repo.h"
#include <stdlib.h>
#include <string.h>

#define REPO repo_get_instance()

#define SEND_RESULT(db_call, success_status) \
    do { \
        DbResult _r = (db_call); \
        if (_r.error) { \
            send_error_json(c, 500, "DB_ERROR", _r.error); \
            free(_r.error); \
        } else { \
            send_json(c, (success_status), _r.json); \
            free(_r.json); \
        } \
    } while(0)

void handle_get_config(struct mg_connection *c, struct mg_http_message *hm, dp_db_t db) {
    (void)hm;
    SEND_RESULT(REPO->get_config(db), 200);
}

void handle_put_config(struct mg_connection *c, struct mg_http_message *hm, dp_db_t db) {
    char *body = body_to_str(hm);
    if (!body) { send_error_json(c, 400, "BAD_REQUEST", "Body ausente."); return; }
    SEND_RESULT(REPO->update_config(db, body), 200);
    free(body);
}
