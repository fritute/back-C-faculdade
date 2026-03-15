#include "distribpro/http.h"
#include "distribpro/common.h"
#include "distribpro/db.h"
#include <stdlib.h>
#include <stdio.h>

/* ── Produtos ── */
void handle_get_produtos(struct mg_connection *c, struct mg_http_message *hm, dp_db_t db);
void handle_get_produto_by_id(struct mg_connection *c, struct mg_http_message *hm, dp_db_t db);
void handle_post_produtos(struct mg_connection *c, struct mg_http_message *hm, dp_db_t db);
void handle_put_produto(struct mg_connection *c, struct mg_http_message *hm, dp_db_t db);
void handle_patch_produto(struct mg_connection *c, struct mg_http_message *hm, dp_db_t db);
void handle_delete_produto(struct mg_connection *c, struct mg_http_message *hm, dp_db_t db);
void handle_get_estoque_baixo(struct mg_connection *c, struct mg_http_message *hm, dp_db_t db);
/* ── Clientes ── */
void handle_get_clientes(struct mg_connection *c, struct mg_http_message *hm, dp_db_t db);
void handle_get_cliente_by_id(struct mg_connection *c, struct mg_http_message *hm, dp_db_t db);
void handle_post_clientes(struct mg_connection *c, struct mg_http_message *hm, dp_db_t db);
void handle_put_cliente(struct mg_connection *c, struct mg_http_message *hm, dp_db_t db);
void handle_delete_cliente(struct mg_connection *c, struct mg_http_message *hm, dp_db_t db);
/* ── Fornecedores ── */
void handle_get_fornecedores(struct mg_connection *c, struct mg_http_message *hm, dp_db_t db);
void handle_get_fornecedor_by_id(struct mg_connection *c, struct mg_http_message *hm, dp_db_t db);
void handle_post_fornecedores(struct mg_connection *c, struct mg_http_message *hm, dp_db_t db);
void handle_put_fornecedor(struct mg_connection *c, struct mg_http_message *hm, dp_db_t db);
void handle_delete_fornecedor(struct mg_connection *c, struct mg_http_message *hm, dp_db_t db);
/* ── Pedidos ── */
void handle_get_pedidos(struct mg_connection *c, struct mg_http_message *hm, dp_db_t db);
void handle_get_pedido_by_id(struct mg_connection *c, struct mg_http_message *hm, dp_db_t db);
void handle_post_pedidos(struct mg_connection *c, struct mg_http_message *hm, dp_db_t db);
void handle_put_pedido(struct mg_connection *c, struct mg_http_message *hm, dp_db_t db);
void handle_patch_pedido_status(struct mg_connection *c, struct mg_http_message *hm, dp_db_t db);
void handle_delete_pedido(struct mg_connection *c, struct mg_http_message *hm, dp_db_t db);
void handle_get_pedidos_recentes(struct mg_connection *c, struct mg_http_message *hm, dp_db_t db);
/* ── Dashboard ── */
void handle_get_dashboard_kpis(struct mg_connection *c, struct mg_http_message *hm, dp_db_t db);
void handle_get_dashboard_entregas(struct mg_connection *c, struct mg_http_message *hm, dp_db_t db);
void handle_get_dashboard_status_pedidos(struct mg_connection *c, struct mg_http_message *hm, dp_db_t db);
/* ── Estoque ── */
void handle_get_estoque(struct mg_connection *c, struct mg_http_message *hm, dp_db_t db);
void handle_patch_estoque(struct mg_connection *c, struct mg_http_message *hm, dp_db_t db);
/* ── Auth ── */
void handle_post_login(struct mg_connection *c, struct mg_http_message *hm, dp_db_t db);
void handle_post_logout(struct mg_connection *c, struct mg_http_message *hm, dp_db_t db);
void handle_get_me(struct mg_connection *c, struct mg_http_message *hm, dp_db_t db);
void handle_put_perfil(struct mg_connection *c, struct mg_http_message *hm, dp_db_t db);
void handle_put_senha(struct mg_connection *c, struct mg_http_message *hm, dp_db_t db);
void handle_post_register(struct mg_connection *c, struct mg_http_message *hm, dp_db_t db);
void handle_post_register_cliente(struct mg_connection *c, struct mg_http_message *hm, dp_db_t db);
void handle_post_register_fornecedor(struct mg_connection *c, struct mg_http_message *hm, dp_db_t db);
/* ── Meus Pedidos (area cliente) ── */
void handle_get_meus_pedidos(struct mg_connection *c, struct mg_http_message *hm, dp_db_t db);
void handle_get_meu_pedido_by_id(struct mg_connection *c, struct mg_http_message *hm, dp_db_t db);
void handle_get_meu_pedido_status(struct mg_connection *c, struct mg_http_message *hm, dp_db_t db);
/* ── Relatorios ── */
void handle_get_relatorio_vendas(struct mg_connection *c, struct mg_http_message *hm, dp_db_t db);
void handle_get_relatorio_vendas_produto(struct mg_connection *c, struct mg_http_message *hm, dp_db_t db);
void handle_get_relatorio_vendas_cliente(struct mg_connection *c, struct mg_http_message *hm, dp_db_t db);
void handle_get_relatorio_estoque(struct mg_connection *c, struct mg_http_message *hm, dp_db_t db);
void handle_get_relatorio_financeiro(struct mg_connection *c, struct mg_http_message *hm, dp_db_t db);
/* ── Config ── */
void handle_get_config(struct mg_connection *c, struct mg_http_message *hm, dp_db_t db);
void handle_put_config(struct mg_connection *c, struct mg_http_message *hm, dp_db_t db);

int main(void) {
    dp_log_info("Starting DistribPro Professional Backend...");

    /* Lê a string de conexão da variável de ambiente DATABASE_URL.
       Se não estiver definida, usa o fallback local para desenvolvimento. */
    const char *conn_str = getenv("DATABASE_URL");
    static char conn_buf[1024];
    if (!conn_str || conn_str[0] == '\0') {
        conn_str = "host=db.mfftiyyfyojecxhiodro.supabase.co port=6543 dbname=postgres user=postgres password=987046715Gustavo sslmode=require";
    } else if (strstr(conn_str, "postgresql://") && !strstr(conn_str, "sslmode=")) {
        /* Garante sslmode=require para Supabase em formato URI */
        snprintf(conn_buf, sizeof(conn_buf), "%s?sslmode=require", conn_str);
        conn_str = conn_buf;
    }

    dp_db_t db = db_init(conn_str);
    if (!db) { dp_log_error("Failed to connect to database. Exiting."); return 1; }
    dp_log_info("Database connection established.");

    Route routes[] = {
        /* Health check para o Render */
        {"GET",    "/",                                 handle_get_clientes}, /* reusa handler inofensivo */
        {"GET",    "/health",                          handle_get_clientes},
        /* estoque-baixo ANTES de produtos wildcard */
        {"GET",    "/api/v1/produtos/estoque-baixo",   handle_get_estoque_baixo},
        /* Produtos */
        {"GET",    "/api/v1/produtos",                 handle_get_produtos},
        {"POST",   "/api/v1/produtos",                 handle_post_produtos},
        {"GET",    "/api/v1/produtos/*",               handle_get_produto_by_id},
        {"PUT",    "/api/v1/produtos/*",               handle_put_produto},
        {"PATCH",  "/api/v1/produtos/*",               handle_patch_produto},
        {"DELETE", "/api/v1/produtos/*",               handle_delete_produto},
        /* Clientes */
        {"GET",    "/api/v1/clientes",                 handle_get_clientes},
        {"POST",   "/api/v1/clientes",                 handle_post_clientes},
        {"GET",    "/api/v1/clientes/*",               handle_get_cliente_by_id},
        {"PUT",    "/api/v1/clientes/*",               handle_put_cliente},
        {"DELETE", "/api/v1/clientes/*",               handle_delete_cliente},
        /* Fornecedores */
        {"GET",    "/api/v1/fornecedores",             handle_get_fornecedores},
        {"POST",   "/api/v1/fornecedores",             handle_post_fornecedores},
        {"GET",    "/api/v1/fornecedores/*",           handle_get_fornecedor_by_id},
        {"PUT",    "/api/v1/fornecedores/*",           handle_put_fornecedor},
        {"DELETE", "/api/v1/fornecedores/*",           handle_delete_fornecedor},
        /* recentes ANTES de pedidos wildcard */
        {"GET",    "/api/v1/pedidos/recentes",         handle_get_pedidos_recentes},
        /* Pedidos */
        {"GET",    "/api/v1/pedidos",                  handle_get_pedidos},
        {"POST",   "/api/v1/pedidos",                  handle_post_pedidos},
        {"PATCH",  "/api/v1/pedidos/*/status",         handle_patch_pedido_status},
        {"GET",    "/api/v1/pedidos/*",                handle_get_pedido_by_id},
        {"PUT",    "/api/v1/pedidos/*",                handle_put_pedido},
        {"DELETE", "/api/v1/pedidos/*",                handle_delete_pedido},
        /* Dashboard */
        {"GET",    "/api/v1/dashboard/kpis",           handle_get_dashboard_kpis},
        {"GET",    "/api/v1/dashboard/entregas",       handle_get_dashboard_entregas},
        {"GET",    "/api/v1/dashboard/status-pedidos", handle_get_dashboard_status_pedidos},
        /* Estoque */
        {"GET",    "/api/v1/estoque",                  handle_get_estoque},
        {"PATCH",  "/api/v1/estoque/*",                handle_patch_estoque},
        /* Auth */
        {"POST",   "/api/v1/auth/register",            handle_post_register},
        {"POST",   "/api/v1/auth/register-cliente",     handle_post_register_cliente},
        {"POST",   "/api/v1/auth/register-fornecedor",  handle_post_register_fornecedor},
        {"POST",   "/api/v1/auth/login",               handle_post_login},
        {"POST",   "/api/v1/auth/logout",              handle_post_logout},
        {"GET",    "/api/v1/auth/me",                  handle_get_me},
        {"PUT",    "/api/v1/auth/perfil",              handle_put_perfil},
        {"PUT",    "/api/v1/auth/senha",               handle_put_senha},
        /* Meus Pedidos (area do cliente) */
        {"GET",    "/api/v1/meus-pedidos",             handle_get_meus_pedidos},
        {"GET",    "/api/v1/meus-pedidos/*/status",    handle_get_meu_pedido_status},
        {"GET",    "/api/v1/meus-pedidos/*",           handle_get_meu_pedido_by_id},
        /* Relatorios */
        {"GET",    "/api/v1/relatorios/vendas",           handle_get_relatorio_vendas},
        {"GET",    "/api/v1/relatorios/vendas/produtos",  handle_get_relatorio_vendas_produto},
        {"GET",    "/api/v1/relatorios/vendas/clientes",  handle_get_relatorio_vendas_cliente},
        {"GET",    "/api/v1/relatorios/estoque",          handle_get_relatorio_estoque},
        {"GET",    "/api/v1/relatorios/financeiro",       handle_get_relatorio_financeiro},
        /* Config */
        {"GET",    "/api/v1/config",                   handle_get_config},
        {"PUT",    "/api/v1/config",                   handle_put_config},
    };

    /* Lê a porta da variável PORT (injetada pelo Render) ou usa 8000 como padrão */
    const char *port_env = getenv("PORT");
    char listen_addr[32];
    snprintf(listen_addr, sizeof(listen_addr), "http://0.0.0.0:%s", port_env ? port_env : "8000");
    dp_log_info(listen_addr);

    start_http_server(listen_addr, routes, sizeof(routes) / sizeof(routes[0]), db);
    db_close(db);
    return 0;
}
