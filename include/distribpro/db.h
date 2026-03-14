#ifndef DISTRIBPRO_DB_H
#define DISTRIBPRO_DB_H

#include "models.h"

/* Resultado de qualquer operação DB.
   Em sucesso: json != NULL, error == NULL.
   Em erro:    json == NULL, error != NULL (string "NOT_FOUND" para 404). */
typedef struct {
    char *json;   /* resposta JSON completa (caller deve free) */
    char *error;  /* mensagem de erro     (caller deve free) */
} DbResult;

typedef void* dp_db_t;

typedef struct {
    /* ---- Produtos ---- */
    DbResult (*get_produtos)(dp_db_t db);
    DbResult (*get_produto_by_id)(dp_db_t db, int id);
    DbResult (*save_produto)(dp_db_t db, const char *body);
    DbResult (*update_produto)(dp_db_t db, int id, const char *body);
    DbResult (*delete_produto)(dp_db_t db, int id);
    DbResult (*get_estoque_baixo)(dp_db_t db);
    /* ---- Clientes ---- */
    DbResult (*get_clientes)(dp_db_t db);
    DbResult (*get_cliente_by_id)(dp_db_t db, int id);
    DbResult (*save_cliente)(dp_db_t db, const char *body);
    DbResult (*update_cliente)(dp_db_t db, int id, const char *body);
    DbResult (*delete_cliente)(dp_db_t db, int id);
    /* ---- Fornecedores ---- */
    DbResult (*get_fornecedores)(dp_db_t db);
    DbResult (*get_fornecedor_by_id)(dp_db_t db, int id);
    DbResult (*save_fornecedor)(dp_db_t db, const char *body);
    DbResult (*update_fornecedor)(dp_db_t db, int id, const char *body);
    DbResult (*delete_fornecedor)(dp_db_t db, int id);
    /* ---- Pedidos ---- */
    DbResult (*get_pedidos)(dp_db_t db);
    DbResult (*get_pedido_by_id)(dp_db_t db, int id);
    DbResult (*save_pedido)(dp_db_t db, const char *body);
    DbResult (*update_pedido)(dp_db_t db, int id, const char *body);
    DbResult (*update_pedido_status)(dp_db_t db, int id, const char *status);
    DbResult (*delete_pedido)(dp_db_t db, int id);
    DbResult (*get_pedidos_recentes)(dp_db_t db);
    /* ---- Dashboard ---- */
    DbResult (*get_dashboard_kpis)(dp_db_t db);
    DbResult (*get_dashboard_entregas)(dp_db_t db, int dias);
    DbResult (*get_dashboard_status_pedidos)(dp_db_t db);
    /* ---- Estoque ---- */
    DbResult (*get_estoque)(dp_db_t db);
    DbResult (*ajustar_estoque)(dp_db_t db, int produto_id, int qtd);
    /* ---- Auth / Usuários ---- */
    DbResult (*get_usuario_by_email)(dp_db_t db, const char *email);
    DbResult (*get_usuario_by_id)(dp_db_t db, int id);
    DbResult (*update_usuario_perfil)(dp_db_t db, int id, const char *nome, const char *new_email);
    DbResult (*update_usuario_senha)(dp_db_t db, int id, const char *senha_hash);
    /* ---- Config ---- */
    DbResult (*get_config)(dp_db_t db);
    DbResult (*update_config)(dp_db_t db, const char *body);
} Repository;

Repository* get_repository(const char *conn_str);
Repository* repo_get_instance(void);

dp_db_t db_init(const char *conn_str);
void db_close(dp_db_t db);
void* db_get_pg_conn(dp_db_t db);

#endif // DISTRIBPRO_DB_H
