#include "distribpro/http.h"
#include "distribpro/common.h"
#include "cJSON.h"
#include <string.h>
#include <stdlib.h>

typedef struct {
    Route *routes;
    int num_routes;
    dp_db_t db;
} server_data;

/* ── helpers ──────────────────────────────────────────────── */

void send_json(struct mg_connection *c, int status, const char *json) {
    mg_http_reply(c, status,
        "Content-Type: application/json\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "Access-Control-Allow-Methods: GET,POST,PUT,PATCH,DELETE,OPTIONS\r\n"
        "Access-Control-Allow-Headers: Content-Type,Authorization\r\n",
        "%s", json);
}

void send_error_json(struct mg_connection *c, int status, const char *code, const char *msg) {
    char buf[1024];
    snprintf(buf, sizeof(buf),
        "{\"success\":false,\"error\":{\"code\":\"%s\",\"message\":\"%s\"}}",
        code ? code : "ERROR", msg ? msg : "Erro interno.");
    send_json(c, status, buf);
}

int extract_id_from_uri(struct mg_http_message *hm) {
    const char *p = hm->uri.buf + hm->uri.len;
    while (p > hm->uri.buf && *(p - 1) != '/') p--;
    return atoi(p);
}

/* Para URIs como /api/v1/pedidos/5/status — retorna 5 */
int extract_segment_before_last(struct mg_http_message *hm) {
    const char *end = hm->uri.buf + hm->uri.len;
    const char *p = end - 1;
    while (p > hm->uri.buf && *p != '/') p--;   /* skip last segment */
    p--;                                          /* skip the '/' */
    const char *q = p;
    while (q > hm->uri.buf && *(q - 1) != '/') q--;
    char buf[32] = {0};
    int len = (int)(p - q + 1);
    if (len < 1 || len > 31) return -1;
    strncpy(buf, q, (size_t)len);
    return atoi(buf);
}

char *body_to_str(struct mg_http_message *hm) {
    if (hm->body.len == 0) return NULL;
    char *s = malloc(hm->body.len + 1);
    if (!s) return NULL;
    memcpy(s, hm->body.buf, hm->body.len);
    s[hm->body.len] = '\0';
    return s;
}

const char *get_bearer_token(struct mg_http_message *hm, char *buf, size_t buf_len) {
    struct mg_str *auth = mg_http_get_header(hm, "Authorization");
    if (!auth || auth->len < 8) return NULL;
    if (strncasecmp(auth->buf, "Bearer ", 7) != 0) return NULL;
    size_t len = auth->len - 7;
    if (len >= buf_len) return NULL;
    memcpy(buf, auth->buf + 7, len);
    buf[len] = '\0';
    return buf;
}

/* ── mongoose compat ─────────────────────────────────────── */

int mg_vcasecmp(const struct mg_str *str1, const char *str2) {
    return mg_strcasecmp(*str1, mg_str(str2));
}

bool mg_http_match_uri(const struct mg_http_message *hm, const char *glob) {
    return mg_match(hm->uri, mg_str(glob), NULL);
}

/* ── legacy send_api_response (mantido para compat) ─────── */

void send_api_response(struct mg_connection *c, int status, ApiResponse *res) {
    cJSON *root = cJSON_CreateObject();
    cJSON_AddBoolToObject(root, "success", res->success);

    if (res->json_data) {
        cJSON *data = cJSON_Parse(res->json_data);
        if (data) cJSON_AddItemToObject(root, "data", data);
    }

    if (!res->success) {
        cJSON *error = cJSON_AddObjectToObject(root, "error");
        cJSON_AddStringToObject(error, "code",    res->error_code    ? res->error_code    : "UNKNOWN_ERROR");
        cJSON_AddStringToObject(error, "message", res->error_message ? res->error_message : "Unknown error occurred");
    }

    char *json_out = cJSON_PrintUnformatted(root);
    send_json(c, status, json_out);
    free(json_out);
    cJSON_Delete(root);
}

/* ── event handler ───────────────────────────────────────── */

static void ev_handler(struct mg_connection *c, int ev, void *ev_data) {
    server_data *data = (server_data *)c->fn_data;
    if (ev == MG_EV_HTTP_MSG) {
        struct mg_http_message *hm = (struct mg_http_message *)ev_data;

        /* Preflight CORS */
        if (mg_vcasecmp(&hm->method, "OPTIONS") == 0) {
            mg_http_reply(c, 204,
                "Access-Control-Allow-Origin: *\r\n"
                "Access-Control-Allow-Methods: GET,POST,PUT,PATCH,DELETE,OPTIONS\r\n"
                "Access-Control-Allow-Headers: Content-Type,Authorization\r\n",
                "");
            return;
        }

        bool found = false;
        for (int i = 0; i < data->num_routes; i++) {
            if (mg_vcasecmp(&hm->method, data->routes[i].method) == 0 &&
                mg_http_match_uri(hm, data->routes[i].path_pattern)) {
                data->routes[i].handler(c, hm, data->db);
                found = true;
                break;
            }
        }

        if (!found) {
            send_error_json(c, 404, "NOT_FOUND", "Recurso não encontrado.");
        }
    }
}

void start_http_server(const char *addr, Route *routes, int num_routes, dp_db_t db) {
    server_data *data = (server_data *)malloc(sizeof(server_data));
    if (!data) { dp_log_error("Failed to allocate server_data"); return; }
    data->routes    = routes;
    data->num_routes = num_routes;
    data->db        = db;

    struct mg_mgr mgr;
    mg_mgr_init(&mgr);

    if (mg_http_listen(&mgr, addr, ev_handler, data) == NULL) {
        dp_log_error("Failed to start HTTP server on %s", addr);
        free(data);
        return;
    }

    dp_log_info("DistribPro Backend listening on %s", addr);
    for (;;) mg_mgr_poll(&mgr, 1000);
    mg_mgr_free(&mgr);
    free(data);
}
