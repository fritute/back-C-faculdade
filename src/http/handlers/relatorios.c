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

/* Helper: extrai query params inicio, fim e limite da URI */
static void parse_relatorio_params(struct mg_http_message *hm,
                                    char *inicio, size_t isz,
                                    char *fim, size_t fsz,
                                    int *limite) {
    struct mg_str q = hm->query;
    char buf[64];

    if (mg_http_get_var(&q, "inicio", buf, sizeof(buf)) > 0) snprintf(inicio, isz, "%s", buf);
    else snprintf(inicio, isz, "2000-01-01");

    if (mg_http_get_var(&q, "fim", buf, sizeof(buf)) > 0) snprintf(fim, fsz, "%s", buf);
    else snprintf(fim, fsz, "2099-12-31");

    if (mg_http_get_var(&q, "limite", buf, sizeof(buf)) > 0) *limite = atoi(buf);
    else *limite = 10;
}

void handle_get_relatorio_vendas(struct mg_connection *c, struct mg_http_message *hm, dp_db_t db) {
    char inicio[32], fim[32]; int limite;
    parse_relatorio_params(hm, inicio, sizeof(inicio), fim, sizeof(fim), &limite);
    SEND_RESULT(REPO->relatorio_vendas(db, inicio, fim), 200);
}

void handle_get_relatorio_vendas_produto(struct mg_connection *c, struct mg_http_message *hm, dp_db_t db) {
    char inicio[32], fim[32]; int limite;
    parse_relatorio_params(hm, inicio, sizeof(inicio), fim, sizeof(fim), &limite);
    SEND_RESULT(REPO->relatorio_vendas_por_produto(db, inicio, fim, limite), 200);
}

void handle_get_relatorio_vendas_cliente(struct mg_connection *c, struct mg_http_message *hm, dp_db_t db) {
    char inicio[32], fim[32]; int limite;
    parse_relatorio_params(hm, inicio, sizeof(inicio), fim, sizeof(fim), &limite);
    SEND_RESULT(REPO->relatorio_vendas_por_cliente(db, inicio, fim, limite), 200);
}

void handle_get_relatorio_estoque(struct mg_connection *c, struct mg_http_message *hm, dp_db_t db) {
    (void)hm;
    SEND_RESULT(REPO->relatorio_estoque(db), 200);
}

void handle_get_relatorio_financeiro(struct mg_connection *c, struct mg_http_message *hm, dp_db_t db) {
    char inicio[32], fim[32]; int limite;
    parse_relatorio_params(hm, inicio, sizeof(inicio), fim, sizeof(fim), &limite);
    SEND_RESULT(REPO->relatorio_financeiro(db, inicio, fim), 200);
}
