#ifndef DISTRIBPRO_HTTP_H
#define DISTRIBPRO_HTTP_H

#include "mongoose.h"
#include "common.h"
#include "distribpro/db.h"

// Mongoose compatibility helpers
int mg_vcasecmp(const struct mg_str *str1, const char *str2);
bool mg_http_match_uri(const struct mg_http_message *hm, const char *glob);

typedef void (*RequestHandler)(struct mg_connection *c, struct mg_http_message *hm, dp_db_t db);

typedef struct {
    const char *method;
    const char *path_pattern;
    RequestHandler handler;
} Route;

void start_http_server(const char *listen_addr, Route *routes, int num_routes, dp_db_t db);
void send_api_response(struct mg_connection *c, int status, ApiResponse *res);

/* Helpers usados pelos handlers */
void send_json(struct mg_connection *c, int status, const char *json);
void send_error_json(struct mg_connection *c, int status, const char *code, const char *msg);
int  extract_id_from_uri(struct mg_http_message *hm);
int  extract_segment_before_last(struct mg_http_message *hm);
char *body_to_str(struct mg_http_message *hm);
const char *get_bearer_token(struct mg_http_message *hm, char *buf, size_t buf_len);

#endif // DISTRIBPRO_HTTP_H
