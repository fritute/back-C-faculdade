#include "distribpro/http.h"
#include "distribpro/db.h"
#include "distribpro/repo.h"
#include "cJSON.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define REPO repo_get_instance()

/* ── Gerenciamento simples de sessões em memória ─────────── */

#define MAX_SESSIONS 200

typedef struct {
    char  token[65];
    int   user_id;
    char  role[20];
    time_t expires;
} Session;

static Session s_sessions[MAX_SESSIONS];
static int     s_session_count = 0;

static void generate_token(char out[65]) {
    static unsigned int seed = 0;
    if (!seed) seed = (unsigned)time(NULL);
    for (int i = 0; i < 64; i += 2) {
        seed = seed * 1103515245u + 12345u;
        snprintf(out + i, 3, "%02x", (seed >> 16) & 0xff);
    }
    out[64] = '\0';
}

static int store_session(const char *token, int user_id, const char *role) {
    if (s_session_count >= MAX_SESSIONS) s_session_count = 0; /* circular */
    Session *s = &s_sessions[s_session_count++];
    strncpy(s->token, token, 64);
    s->user_id = user_id;
    strncpy(s->role, role ? role : "operador", sizeof(s->role) - 1);
    s->expires = time(NULL) + 86400; /* 24h */
    return 1;
}

static Session *find_session(const char *token) {
    if (!token) return NULL;
    time_t now = time(NULL);
    for (int i = 0; i < s_session_count; i++) {
        if (strcmp(s_sessions[i].token, token) == 0 && s_sessions[i].expires > now)
            return &s_sessions[i];
    }
    return NULL;
}

static void revoke_session(const char *token) {
    for (int i = 0; i < s_session_count; i++) {
        if (strcmp(s_sessions[i].token, token) == 0) {
            s_sessions[i].expires = 0;
            return;
        }
    }
}

/* Retorna user_id do token Bearer, ou -1 se inválido */
int auth_get_current_user(struct mg_http_message *hm) {
    char buf[65];
    const char *tok = get_bearer_token(hm, buf, sizeof(buf));
    if (!tok) return -1;
    Session *s = find_session(tok);
    return s ? s->user_id : -1;
}

/* ── Endpoints ──────────────────────────────────────────── */

void handle_post_register(struct mg_connection *c, struct mg_http_message *hm, dp_db_t db) {
    char *body = body_to_str(hm);
    if (!body) { send_error_json(c, 400, "BAD_REQUEST", "Body ausente."); return; }
    cJSON *j = cJSON_Parse(body);
    if (!j) { free(body); send_error_json(c, 400, "BAD_REQUEST", "JSON inválido."); return; }
    const char *nome  = cJSON_GetStringValue(cJSON_GetObjectItem(j, "nome"));
    const char *email = cJSON_GetStringValue(cJSON_GetObjectItem(j, "email"));
    const char *senha = cJSON_GetStringValue(cJSON_GetObjectItem(j, "senha"));
    if (!nome || !email || !senha) {
        cJSON_Delete(j); free(body);
        send_error_json(c, 400, "VALIDATION_ERROR", "nome, email e senha são obrigatórios.");
        return;
    }
    cJSON_Delete(j);
    DbResult r = REPO->save_usuario(db, body);
    free(body);
    if (r.error) {
        const char *msg = r.error;
        int status = 500;
        if (strstr(msg, "unique") || strstr(msg, "duplicate") || strstr(msg, "already")) status = 409;
        send_error_json(c, status, status == 409 ? "CONFLICT" : "DB_ERROR", msg);
        free(r.error);
        return;
    }
    /* Auto-login: gera token e retorna no mesmo formato do /login */
    cJSON *user_json = cJSON_Parse(r.json);
    free(r.json);
    cJSON *data = cJSON_GetObjectItem(user_json, "data");
    int user_id = (int)cJSON_GetNumberValue(cJSON_GetObjectItem(data, "id"));
    const char *u_nome = cJSON_GetStringValue(cJSON_GetObjectItem(data, "nome"));
    const char *u_role = cJSON_GetStringValue(cJSON_GetObjectItem(data, "role"));

    char token[65];
    generate_token(token);
    store_session(token, user_id, u_role);

    cJSON *resp = cJSON_CreateObject();
    cJSON_AddBoolToObject(resp, "success", 1);
    cJSON *d = cJSON_AddObjectToObject(resp, "data");
    cJSON_AddStringToObject(d, "token", token);
    cJSON_AddNumberToObject(d, "userId", user_id);
    cJSON_AddStringToObject(d, "nome", u_nome ? u_nome : "");
    cJSON_AddStringToObject(d, "role", u_role ? u_role : "operador");
    char *out = cJSON_PrintUnformatted(resp);
    send_json(c, 201, out);
    free(out);
    cJSON_Delete(resp);
    cJSON_Delete(user_json);
}

void handle_post_login(struct mg_connection *c, struct mg_http_message *hm, dp_db_t db) {
    char *body = body_to_str(hm);
    if (!body) { send_error_json(c, 400, "BAD_REQUEST", "Body ausente."); return; }
    cJSON *j = cJSON_Parse(body);
    free(body);
    if (!j) { send_error_json(c, 400, "BAD_REQUEST", "JSON inválido."); return; }
    const char *email = cJSON_GetStringValue(cJSON_GetObjectItem(j, "email"));
    const char *senha_raw = cJSON_GetStringValue(cJSON_GetObjectItem(j, "senha"));
    if (!email || !senha_raw) {
        cJSON_Delete(j);
        send_error_json(c, 400, "VALIDATION_ERROR", "email e senha são obrigatórios.");
        return;
    }
    /* Copia a senha antes de liberar o JSON de entrada */
    char senha[256];
    snprintf(senha, sizeof(senha), "%s", senha_raw);
    DbResult r = REPO->get_usuario_by_email(db, email);
    cJSON_Delete(j);
    if (r.error) {
        int s = strcmp(r.error, "NOT_FOUND") == 0 ? 401 : 500;
        send_error_json(c, s, "UNAUTHORIZED", "Credenciais inválidas.");
        free(r.error);
        return;
    }
    /* Comparar senha com senha_hash armazenada */
    cJSON *user_json = cJSON_Parse(r.json);
    free(r.json);
    cJSON *data = cJSON_GetObjectItem(user_json, "data");
    const char *stored_hash = cJSON_GetStringValue(cJSON_GetObjectItem(data, "senha_hash"));
    int user_id = (int)cJSON_GetNumberValue(cJSON_GetObjectItem(data, "id"));
    const char *role = cJSON_GetStringValue(cJSON_GetObjectItem(data, "role"));
    const char *nome = cJSON_GetStringValue(cJSON_GetObjectItem(data, "nome"));

    if (!stored_hash || strcmp(stored_hash, senha) != 0) {
        printf("[AUTH] Falha de login para '%s': Senha incorreta ou usuario sem hash.\n", email);
        cJSON_Delete(user_json);
        send_error_json(c, 401, "UNAUTHORIZED", "Credenciais inválidas.");
        return;
    }

    char token[65];
    generate_token(token);
    store_session(token, user_id, role);

    cJSON *resp = cJSON_CreateObject();
    cJSON_AddBoolToObject(resp, "success", 1);
    cJSON *d = cJSON_AddObjectToObject(resp, "data");
    cJSON_AddStringToObject(d, "token", token);
    cJSON_AddNumberToObject(d, "userId", user_id);
    cJSON_AddStringToObject(d, "nome", nome ? nome : "");
    cJSON_AddStringToObject(d, "role", role ? role : "operador");
    char *out = cJSON_PrintUnformatted(resp);
    send_json(c, 200, out);
    free(out);
    cJSON_Delete(resp);
    cJSON_Delete(user_json);
}

void handle_post_logout(struct mg_connection *c, struct mg_http_message *hm, dp_db_t db) {
    (void)db;
    char buf[65];
    const char *tok = get_bearer_token(hm, buf, sizeof(buf));
    if (tok) revoke_session(tok);
    send_json(c, 200, "{\"success\":true,\"data\":{\"message\":\"Logout realizado com sucesso.\"}}");
}

void handle_get_me(struct mg_connection *c, struct mg_http_message *hm, dp_db_t db) {
    int uid = auth_get_current_user(hm);
    if (uid < 0) { send_error_json(c, 401, "UNAUTHORIZED", "Token inválido ou expirado."); return; }
    DbResult r = REPO->get_usuario_by_id(db, uid);
    if (r.error) {
        send_error_json(c, 500, "DB_ERROR", r.error);
        free(r.error);
        return;
    }
    send_json(c, 200, r.json);
    free(r.json);
}

void handle_put_perfil(struct mg_connection *c, struct mg_http_message *hm, dp_db_t db) {
    int uid = auth_get_current_user(hm);
    if (uid < 0) { send_error_json(c, 401, "UNAUTHORIZED", "Token inválido ou expirado."); return; }
    char *body = body_to_str(hm);
    if (!body) { send_error_json(c, 400, "BAD_REQUEST", "Body ausente."); return; }
    cJSON *j = cJSON_Parse(body);
    free(body);
    if (!j) { send_error_json(c, 400, "BAD_REQUEST", "JSON inválido."); return; }
    const char *nome  = cJSON_GetStringValue(cJSON_GetObjectItem(j, "nome"));
    const char *email = cJSON_GetStringValue(cJSON_GetObjectItem(j, "email"));
    if (!nome || !email) { cJSON_Delete(j); send_error_json(c, 400, "VALIDATION_ERROR", "nome e email são obrigatórios."); return; }
    DbResult r = REPO->update_usuario_perfil(db, uid, nome, email);
    cJSON_Delete(j);
    if (r.error) { send_error_json(c, 500, "DB_ERROR", r.error); free(r.error); return; }
    send_json(c, 200, r.json);
    free(r.json);
}

void handle_post_register_cliente(struct mg_connection *c, struct mg_http_message *hm, dp_db_t db) {
    char *body = body_to_str(hm);
    if (!body) { send_error_json(c, 400, "BAD_REQUEST", "Body ausente."); return; }
    cJSON *j = cJSON_Parse(body);
    if (!j) { free(body); send_error_json(c, 400, "BAD_REQUEST", "JSON invalido."); return; }
    const char *nome  = cJSON_GetStringValue(cJSON_GetObjectItem(j, "nome"));
    const char *email = cJSON_GetStringValue(cJSON_GetObjectItem(j, "email"));
    const char *senha = cJSON_GetStringValue(cJSON_GetObjectItem(j, "senha"));
    if (!nome || !email || !senha) {
        cJSON_Delete(j); free(body);
        send_error_json(c, 400, "VALIDATION_ERROR", "nome, email e senha sao obrigatorios.");
        return;
    }
    cJSON_Delete(j);
    DbResult r = REPO->save_usuario_with_role(db, body, "cliente");
    if (r.error) {
        int status = 500;
        if (strstr(r.error, "unique") || strstr(r.error, "duplicate") || strstr(r.error, "already")) status = 409;
        send_error_json(c, status, status == 409 ? "CONFLICT" : "DB_ERROR", r.error);
        free(r.error); free(body);
        return;
    }
    /* Auto-login + cria registro em clientes */
    cJSON *user_json = cJSON_Parse(r.json);
    free(r.json);
    cJSON *data = cJSON_GetObjectItem(user_json, "data");
    int user_id = (int)cJSON_GetNumberValue(cJSON_GetObjectItem(data, "id"));
    const char *u_nome = cJSON_GetStringValue(cJSON_GetObjectItem(data, "nome"));
    const char *u_role = cJSON_GetStringValue(cJSON_GetObjectItem(data, "role"));

    /* Cria perfil cliente vinculado */
    REPO->save_cliente_for_user(db, user_id, body);
    free(body);

    char token[65];
    generate_token(token);
    store_session(token, user_id, u_role);

    cJSON *resp = cJSON_CreateObject();
    cJSON_AddBoolToObject(resp, "success", 1);
    cJSON *d = cJSON_AddObjectToObject(resp, "data");
    cJSON_AddStringToObject(d, "token", token);
    cJSON_AddNumberToObject(d, "userId", user_id);
    cJSON_AddStringToObject(d, "nome", u_nome ? u_nome : "");
    cJSON_AddStringToObject(d, "role", "cliente");
    char *out = cJSON_PrintUnformatted(resp);
    send_json(c, 201, out);
    free(out);
    cJSON_Delete(resp);
    cJSON_Delete(user_json);
}

void handle_post_register_fornecedor(struct mg_connection *c, struct mg_http_message *hm, dp_db_t db) {
    char *body = body_to_str(hm);
    if (!body) { send_error_json(c, 400, "BAD_REQUEST", "Body ausente."); return; }
    cJSON *j = cJSON_Parse(body);
    if (!j) { free(body); send_error_json(c, 400, "BAD_REQUEST", "JSON invalido."); return; }
    const char *nome  = cJSON_GetStringValue(cJSON_GetObjectItem(j, "nome"));
    const char *email = cJSON_GetStringValue(cJSON_GetObjectItem(j, "email"));
    const char *senha = cJSON_GetStringValue(cJSON_GetObjectItem(j, "senha"));
    if (!nome || !email || !senha) {
        cJSON_Delete(j); free(body);
        send_error_json(c, 400, "VALIDATION_ERROR", "nome, email e senha sao obrigatorios.");
        return;
    }
    cJSON_Delete(j);
    DbResult r = REPO->save_usuario_with_role(db, body, "fornecedor");
    if (r.error) {
        int status = 500;
        if (strstr(r.error, "unique") || strstr(r.error, "duplicate") || strstr(r.error, "already")) status = 409;
        send_error_json(c, status, status == 409 ? "CONFLICT" : "DB_ERROR", r.error);
        free(r.error); free(body);
        return;
    }
    /* Auto-login + cria registro em fornecedores */
    cJSON *user_json = cJSON_Parse(r.json);
    free(r.json);
    cJSON *data = cJSON_GetObjectItem(user_json, "data");
    int user_id = (int)cJSON_GetNumberValue(cJSON_GetObjectItem(data, "id"));
    const char *u_nome = cJSON_GetStringValue(cJSON_GetObjectItem(data, "nome"));
    const char *u_role = cJSON_GetStringValue(cJSON_GetObjectItem(data, "role"));

    /* Cria perfil fornecedor vinculado */
    REPO->save_fornecedor_for_user(db, user_id, body);
    free(body);

    char token[65];
    generate_token(token);
    store_session(token, user_id, u_role);

    cJSON *resp = cJSON_CreateObject();
    cJSON_AddBoolToObject(resp, "success", 1);
    cJSON *d = cJSON_AddObjectToObject(resp, "data");
    cJSON_AddStringToObject(d, "token", token);
    cJSON_AddNumberToObject(d, "userId", user_id);
    cJSON_AddStringToObject(d, "nome", u_nome ? u_nome : "");
    cJSON_AddStringToObject(d, "role", "fornecedor");
    char *out = cJSON_PrintUnformatted(resp);
    send_json(c, 201, out);
    free(out);
    cJSON_Delete(resp);
    cJSON_Delete(user_json);
}

void handle_put_senha(struct mg_connection *c, struct mg_http_message *hm, dp_db_t db) {
    int uid = auth_get_current_user(hm);
    if (uid < 0) { send_error_json(c, 401, "UNAUTHORIZED", "Token inválido ou expirado."); return; }
    char *body = body_to_str(hm);
    if (!body) { send_error_json(c, 400, "BAD_REQUEST", "Body ausente."); return; }
    cJSON *j = cJSON_Parse(body);
    free(body);
    if (!j) { send_error_json(c, 400, "BAD_REQUEST", "JSON inválido."); return; }
    const char *nova = cJSON_GetStringValue(cJSON_GetObjectItem(j, "novaSenha"));
    if (!nova) nova = cJSON_GetStringValue(cJSON_GetObjectItem(j, "senha"));
    if (!nova) { cJSON_Delete(j); send_error_json(c, 400, "VALIDATION_ERROR", "Campo 'novaSenha' obrigatório."); return; }
    DbResult r = REPO->update_usuario_senha(db, uid, nova);
    cJSON_Delete(j);
    if (r.error) { send_error_json(c, 500, "DB_ERROR", r.error); free(r.error); return; }
    send_json(c, 200, r.json);
    free(r.json);
}
