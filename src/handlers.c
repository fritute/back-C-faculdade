#include "distribpro.h"
#include "mongoose.h"
#include "cJSON.h"

// Helper to send JSON response
void send_json_response(struct mg_connection *c, int status, const char *json_str) {
    mg_http_reply(c, status, "Content-Type: application/json\r\n", "%s", json_str);
}

void handle_produtos(struct mg_connection *c, int ev, void *ev_data, void *fn_data) {
    struct mg_http_message *hm = (struct mg_http_message *) ev_data;
    
    if (mg_vcasecmp(&hm->method, "GET") == 0) {
        // Mock data for products
        cJSON *root = cJSON_CreateObject();
        cJSON_AddBoolToObject(root, "success", true);
        cJSON *data = cJSON_AddArrayToObject(root, "data");
        
        cJSON *p1 = cJSON_CreateObject();
        cJSON_AddNumberToObject(p1, "id", 1);
        cJSON_AddStringToObject(p1, "nome", "Arroz Tipo 1");
        cJSON_AddStringToObject(p1, "categoria", "Alimentício");
        cJSON_AddNumberToObject(p1, "preco", 89.90);
        cJSON_AddNumberToObject(p1, "estoque", 100);
        cJSON_AddItemToArray(data, p1);

        char *json_out = cJSON_PrintUnformatted(root);
        send_json_response(c, 200, json_out);
        
        free(json_out);
        cJSON_Delete(root);
    } else if (mg_vcasecmp(&hm->method, "POST") == 0) {
        // Logic for POST /produtos
        send_json_response(c, 201, "{\"success\":true, \"message\":\"Produto criado\"}");
    } else {
        send_json_response(c, 405, "{\"success\":false, \"error\":\"Method Not Allowed\"}");
    }
}

void handle_clientes(struct mg_connection *c, int ev, void *ev_data, void *fn_data) {
    send_json_response(c, 200, "{\"success\":true, \"data\":[]}");
}

void handle_fornecedores(struct mg_connection *c, int ev, void *ev_data, void *fn_data) {
    send_json_response(c, 200, "{\"success\":true, \"data\":[]}");
}

void handle_pedidos(struct mg_connection *c, int ev, void *ev_data, void *fn_data) {
    send_json_response(c, 200, "{\"success\":true, \"data\":[]}");
}

void handle_dashboard(struct mg_connection *c, int ev, void *ev_data, void *fn_data) {
    send_json_response(c, 200, "{\"success\":true, \"data\":{\"receitaTotal\":0}}");
}
