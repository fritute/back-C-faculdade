#include "distribpro/http.h"
#include "distribpro/db.h"
#include "distribpro/repo.h"
#include "mongoose.h"
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

void handle_get_dashboard_kpis(struct mg_connection *c, struct mg_http_message *hm, dp_db_t db) {
    (void)hm;
    SEND_RESULT(REPO->get_dashboard_kpis(db), 200);
}

void handle_get_dashboard_entregas(struct mg_connection *c, struct mg_http_message *hm, dp_db_t db) {
    /* Query param: ?dias=7|14|30 */
    char dias_buf[8] = "7";
    mg_http_get_var(&hm->query, "dias", dias_buf, sizeof(dias_buf));
    int dias = atoi(dias_buf);
    if (dias <= 0) dias = 7;
    SEND_RESULT(REPO->get_dashboard_entregas(db, dias), 200);
}

void handle_get_dashboard_status_pedidos(struct mg_connection *c, struct mg_http_message *hm, dp_db_t db) {
    (void)hm;
    SEND_RESULT(REPO->get_dashboard_status_pedidos(db), 200);
}
