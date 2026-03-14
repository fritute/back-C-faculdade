#include "distribpro/db.h"
#include "distribpro/repo.h"
#include "cJSON.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <libpq-fe.h>

static Repository *s_current_repo = NULL;

/* ═══════════════════════════════════════════════════════════
   Helpers internos
   ═══════════════════════════════════════════════════════════ */

/* Converte PGresult para array cJSON (transfere ownership do PGresult) */
static cJSON *pg_to_array(PGresult *res) {
    cJSON *arr = cJSON_CreateArray();
    int rows = PQntuples(res);
    int cols = PQnfields(res);
    for (int i = 0; i < rows; i++) {
        cJSON *obj = cJSON_CreateObject();
        for (int j = 0; j < cols; j++) {
            const char *key = PQfname(res, j);
            if (PQgetisnull(res, i, j)) { cJSON_AddNullToObject(obj, key); continue; }
            const char *v = PQgetvalue(res, i, j);
            Oid t = PQftype(res, j);
            if (t==20||t==21||t==23||t==700||t==701||t==1700)
                cJSON_AddNumberToObject(obj, key, atof(v));
            else if (t == 16)
                cJSON_AddBoolToObject(obj, key, v[0]=='t');
            else
                cJSON_AddStringToObject(obj, key, v);
        }
        cJSON_AddItemToArray(arr, obj);
    }
    return arr;
}

static DbResult ok_list(PGresult *res) {
    cJSON *root = cJSON_CreateObject();
    cJSON_AddBoolToObject(root, "success", 1);
    cJSON_AddItemToObject(root, "data", pg_to_array(res));
    DbResult r = { cJSON_PrintUnformatted(root), NULL };
    cJSON_Delete(root); PQclear(res);
    return r;
}

static DbResult ok_item(PGresult *res) {
    if (PQntuples(res) == 0) {
        PQclear(res);
        DbResult r = { NULL, strdup("NOT_FOUND") };
        return r;
    }
    cJSON *root = cJSON_CreateObject();
    cJSON_AddBoolToObject(root, "success", 1);
    cJSON *arr = pg_to_array(res);
    cJSON *item = cJSON_DetachItemFromArray(arr, 0);
    cJSON_Delete(arr);
    cJSON_AddItemToObject(root, "data", item);
    DbResult r = { cJSON_PrintUnformatted(root), NULL };
    cJSON_Delete(root); PQclear(res);
    return r;
}

static DbResult ok_deleted(PGresult *res) {
    if (res) PQclear(res);
    DbResult r = { strdup("{\"success\":true,\"data\":{\"message\":\"Registro exclu\u00eddo com sucesso.\"}}"), NULL };
    return r;
}

static DbResult db_err(PGconn *conn, PGresult *res) {
    DbResult r = { NULL, strdup(PQerrorMessage(conn)) };
    if (res) PQclear(res);
    return r;
}

#define PG_CONN  PGconn *conn = (PGconn*)db_get_pg_conn(db); \
                 if (!conn) { DbResult r={NULL,strdup("Conex\u00e3o DB indispon\u00edvel.")}; return r; }

/* ═══════════════════════════════════════════════════════════
   PRODUTOS
   ═══════════════════════════════════════════════════════════ */

static DbResult pg_get_produtos(dp_db_t db) {
    PG_CONN
    PGresult *r = PQexec(conn, "SELECT id,nome,categoria,unidade,preco,estoque,estoque_min,sku,"
                               "fornecedor_id,status,descricao,criado_em,atualizado_em "
                               "FROM produtos ORDER BY id");
    if (PQresultStatus(r) != PGRES_TUPLES_OK) return db_err(conn, r);
    return ok_list(r);
}

static DbResult pg_get_produto_by_id(dp_db_t db, int id) {
    PG_CONN
    char sid[16]; snprintf(sid, sizeof(sid), "%d", id);
    const char *p[] = { sid };
    PGresult *r = PQexecParams(conn, "SELECT id,nome,categoria,unidade,preco,estoque,estoque_min,sku,"
                               "fornecedor_id,status,descricao,criado_em,atualizado_em "
                               "FROM produtos WHERE id=$1", 1, NULL, p, NULL, NULL, 0);
    if (PQresultStatus(r) != PGRES_TUPLES_OK) return db_err(conn, r);
    return ok_item(r);
}

static DbResult pg_save_produto(dp_db_t db, const char *body) {
    PG_CONN
    cJSON *j = cJSON_Parse(body);
    if (!j) { DbResult r={NULL,strdup("JSON inv\u00e1lido.")}; return r; }
    const char *nome      = cJSON_GetStringValue(cJSON_GetObjectItem(j,"nome"));
    const char *categoria = cJSON_GetStringValue(cJSON_GetObjectItem(j,"categoria"));
    const char *unidade   = cJSON_GetStringValue(cJSON_GetObjectItem(j,"unidade"));
    const char *descricao = cJSON_GetStringValue(cJSON_GetObjectItem(j,"descricao"));
    const char *sku       = cJSON_GetStringValue(cJSON_GetObjectItem(j,"sku"));
    const char *status_v  = cJSON_GetStringValue(cJSON_GetObjectItem(j,"status"));
    if (!nome || !categoria) { cJSON_Delete(j); DbResult r={NULL,strdup("nome e categoria s\u00e3o obrigat\u00f3rios.")}; return r; }
    char preco_s[32]="0", estoque_s[16]="0", emin_s[16]="10", fid_s[16]="";
    cJSON *pr = cJSON_GetObjectItem(j,"preco");
    if (pr) snprintf(preco_s,sizeof(preco_s),"%.2f",pr->valuedouble);
    cJSON *es = cJSON_GetObjectItem(j,"estoque");
    if (es) snprintf(estoque_s,sizeof(estoque_s),"%d",(int)es->valuedouble);
    cJSON *em = cJSON_GetObjectItem(j,"estoqueMin");
    if (!em) em = cJSON_GetObjectItem(j,"estoque_min");
    if (em) snprintf(emin_s,sizeof(emin_s),"%d",(int)em->valuedouble);
    cJSON *fid = cJSON_GetObjectItem(j,"fornecedor_id");
    const char *fid_p = NULL;
    if (fid && !cJSON_IsNull(fid)) { snprintf(fid_s,sizeof(fid_s),"%d",(int)fid->valuedouble); fid_p=fid_s; }
    const char *p[] = { nome, categoria, unidade?unidade:"Un", preco_s, estoque_s, emin_s,
                        sku?sku:NULL, fid_p, status_v?status_v:"Ativo", descricao?descricao:NULL };
    PGresult *r = PQexecParams(conn,
        "INSERT INTO produtos(nome,categoria,unidade,preco,estoque,estoque_min,sku,fornecedor_id,status,descricao)"
        " VALUES($1,$2,$3,$4,$5,$6,$7,$8,$9,$10)"
        " RETURNING id,nome,categoria,unidade,preco,estoque,estoque_min,sku,fornecedor_id,status,descricao,criado_em,atualizado_em",
        10, NULL, p, NULL, NULL, 0);
    cJSON_Delete(j);
    if (PQresultStatus(r) != PGRES_TUPLES_OK) return db_err(conn, r);
    return ok_item(r);
}

static DbResult pg_update_produto(dp_db_t db, int id, const char *body) {
    PG_CONN
    cJSON *j = cJSON_Parse(body);
    if (!j) { DbResult r={NULL,strdup("JSON inv\u00e1lido.")}; return r; }
    char sid[16]; snprintf(sid,sizeof(sid),"%d",id);
    const char *nome      = cJSON_GetStringValue(cJSON_GetObjectItem(j,"nome"));
    const char *categoria = cJSON_GetStringValue(cJSON_GetObjectItem(j,"categoria"));
    const char *unidade   = cJSON_GetStringValue(cJSON_GetObjectItem(j,"unidade"));
    const char *descricao = cJSON_GetStringValue(cJSON_GetObjectItem(j,"descricao"));
    const char *sku       = cJSON_GetStringValue(cJSON_GetObjectItem(j,"sku"));
    const char *status_v  = cJSON_GetStringValue(cJSON_GetObjectItem(j,"status"));
    char preco_s[32]="0", estoque_s[16]="0", emin_s[16]="10", fid_s[16]="";
    cJSON *pr = cJSON_GetObjectItem(j,"preco");
    if (pr) snprintf(preco_s,sizeof(preco_s),"%.2f",pr->valuedouble);
    cJSON *es = cJSON_GetObjectItem(j,"estoque");
    if (es) snprintf(estoque_s,sizeof(estoque_s),"%d",(int)es->valuedouble);
    cJSON *em = cJSON_GetObjectItem(j,"estoqueMin");
    if (!em) em = cJSON_GetObjectItem(j,"estoque_min");
    if (em) snprintf(emin_s,sizeof(emin_s),"%d",(int)em->valuedouble);
    cJSON *fid = cJSON_GetObjectItem(j,"fornecedor_id");
    const char *fid_p = NULL;
    if (fid && !cJSON_IsNull(fid)) { snprintf(fid_s,sizeof(fid_s),"%d",(int)fid->valuedouble); fid_p=fid_s; }
    const char *p[] = { sid, nome, categoria, unidade?unidade:"Un", preco_s, estoque_s, emin_s,
                        sku?sku:NULL, fid_p, status_v?status_v:"Ativo", descricao?descricao:NULL };
    PGresult *r = PQexecParams(conn,
        "UPDATE produtos SET nome=$2,categoria=$3,unidade=$4,preco=$5,estoque=$6,estoque_min=$7,"
        "sku=$8,fornecedor_id=$9,status=$10,descricao=$11,atualizado_em=NOW() WHERE id=$1"
        " RETURNING id,nome,categoria,unidade,preco,estoque,estoque_min,sku,fornecedor_id,status,descricao,criado_em,atualizado_em",
        11, NULL, p, NULL, NULL, 0);
    cJSON_Delete(j);
    if (PQresultStatus(r) != PGRES_TUPLES_OK) return db_err(conn, r);
    return ok_item(r);
}

static DbResult pg_delete_produto(dp_db_t db, int id) {
    PG_CONN
    char sid[16]; snprintf(sid,sizeof(sid),"%d",id);
    const char *p[] = { sid };
    PGresult *r = PQexecParams(conn,"DELETE FROM produtos WHERE id=$1",1,NULL,p,NULL,NULL,0);
    if (PQresultStatus(r) != PGRES_COMMAND_OK) return db_err(conn, r);
    return ok_deleted(r);
}

static DbResult pg_get_estoque_baixo(dp_db_t db) {
    PG_CONN
    PGresult *r = PQexec(conn,"SELECT id,nome,estoque,estoque_min,status FROM produtos "
                              "WHERE estoque<=estoque_min ORDER BY estoque ASC");
    if (PQresultStatus(r) != PGRES_TUPLES_OK) return db_err(conn, r);
    return ok_list(r);
}

/* ═══════════════════════════════════════════════════════════
   CLIENTES
   ═══════════════════════════════════════════════════════════ */

static DbResult pg_get_clientes(dp_db_t db) {
    PG_CONN
    PGresult *r = PQexec(conn,"SELECT id,nome,tipo,doc,email,tel,cidade,estado,limite,status,criado_em,atualizado_em FROM clientes ORDER BY id");
    if (PQresultStatus(r) != PGRES_TUPLES_OK) return db_err(conn, r);
    return ok_list(r);
}

static DbResult pg_get_cliente_by_id(dp_db_t db, int id) {
    PG_CONN
    char sid[16]; snprintf(sid,sizeof(sid),"%d",id);
    const char *p[]={sid};
    PGresult *r = PQexecParams(conn,"SELECT id,nome,tipo,doc,email,tel,cidade,estado,limite,status,criado_em,atualizado_em FROM clientes WHERE id=$1",1,NULL,p,NULL,NULL,0);
    if (PQresultStatus(r) != PGRES_TUPLES_OK) return db_err(conn, r);
    return ok_item(r);
}

static DbResult pg_save_cliente(dp_db_t db, const char *body) {
    PG_CONN
    cJSON *j = cJSON_Parse(body);
    if (!j) { DbResult r={NULL,strdup("JSON inv\u00e1lido.")}; return r; }
    const char *nome   = cJSON_GetStringValue(cJSON_GetObjectItem(j,"nome"));
    const char *email  = cJSON_GetStringValue(cJSON_GetObjectItem(j,"email"));
    if (!nome||!email) { cJSON_Delete(j); DbResult r={NULL,strdup("nome e email s\u00e3o obrigat\u00f3rios.")}; return r; }
    const char *tipo   = cJSON_GetStringValue(cJSON_GetObjectItem(j,"tipo"));
    const char *doc    = cJSON_GetStringValue(cJSON_GetObjectItem(j,"doc"));
    const char *tel    = cJSON_GetStringValue(cJSON_GetObjectItem(j,"tel"));
    const char *cidade = cJSON_GetStringValue(cJSON_GetObjectItem(j,"cidade"));
    const char *estado = cJSON_GetStringValue(cJSON_GetObjectItem(j,"estado"));
    const char *status_v = cJSON_GetStringValue(cJSON_GetObjectItem(j,"status"));
    char limite_s[32]="0";
    cJSON *lim = cJSON_GetObjectItem(j,"limite");
    if (lim) snprintf(limite_s,sizeof(limite_s),"%.2f",lim->valuedouble);
    const char *p[]={nome,tipo?tipo:"PJ",doc,email,tel,cidade,estado,limite_s,status_v?status_v:"Ativo"};
    PGresult *r = PQexecParams(conn,
        "INSERT INTO clientes(nome,tipo,doc,email,tel,cidade,estado,limite,status)"
        " VALUES($1,$2,$3,$4,$5,$6,$7,$8,$9)"
        " RETURNING id,nome,tipo,doc,email,tel,cidade,estado,limite,status,criado_em,atualizado_em",
        9,NULL,p,NULL,NULL,0);
    cJSON_Delete(j);
    if (PQresultStatus(r) != PGRES_TUPLES_OK) return db_err(conn, r);
    return ok_item(r);
}

static DbResult pg_update_cliente(dp_db_t db, int id, const char *body) {
    PG_CONN
    cJSON *j = cJSON_Parse(body);
    if (!j) { DbResult r={NULL,strdup("JSON inv\u00e1lido.")}; return r; }
    char sid[16]; snprintf(sid,sizeof(sid),"%d",id);
    const char *nome   = cJSON_GetStringValue(cJSON_GetObjectItem(j,"nome"));
    const char *email  = cJSON_GetStringValue(cJSON_GetObjectItem(j,"email"));
    const char *tipo   = cJSON_GetStringValue(cJSON_GetObjectItem(j,"tipo"));
    const char *doc    = cJSON_GetStringValue(cJSON_GetObjectItem(j,"doc"));
    const char *tel    = cJSON_GetStringValue(cJSON_GetObjectItem(j,"tel"));
    const char *cidade = cJSON_GetStringValue(cJSON_GetObjectItem(j,"cidade"));
    const char *estado = cJSON_GetStringValue(cJSON_GetObjectItem(j,"estado"));
    const char *status_v = cJSON_GetStringValue(cJSON_GetObjectItem(j,"status"));
    char limite_s[32]="0";
    cJSON *lim=cJSON_GetObjectItem(j,"limite");
    if (lim) snprintf(limite_s,sizeof(limite_s),"%.2f",lim->valuedouble);
    const char *p[]={sid,nome,tipo?tipo:"PJ",doc,email,tel,cidade,estado,limite_s,status_v?status_v:"Ativo"};
    PGresult *r = PQexecParams(conn,
        "UPDATE clientes SET nome=$2,tipo=$3,doc=$4,email=$5,tel=$6,cidade=$7,estado=$8,limite=$9,status=$10,atualizado_em=NOW() WHERE id=$1"
        " RETURNING id,nome,tipo,doc,email,tel,cidade,estado,limite,status,criado_em,atualizado_em",
        10,NULL,p,NULL,NULL,0);
    cJSON_Delete(j);
    if (PQresultStatus(r) != PGRES_TUPLES_OK) return db_err(conn, r);
    return ok_item(r);
}

static DbResult pg_delete_cliente(dp_db_t db, int id) {
    PG_CONN
    char sid[16]; snprintf(sid,sizeof(sid),"%d",id);
    const char *p[]={sid};
    PGresult *r = PQexecParams(conn,"DELETE FROM clientes WHERE id=$1",1,NULL,p,NULL,NULL,0);
    if (PQresultStatus(r) != PGRES_COMMAND_OK) return db_err(conn, r);
    return ok_deleted(r);
}

/* ═══════════════════════════════════════════════════════════
   FORNECEDORES
   ═══════════════════════════════════════════════════════════ */

static DbResult pg_get_fornecedores(dp_db_t db) {
    PG_CONN
    PGresult *r = PQexec(conn,"SELECT id,nome,cnpj,contato,email,tel,categoria,prazo,status,criado_em,atualizado_em FROM fornecedores ORDER BY id");
    if (PQresultStatus(r) != PGRES_TUPLES_OK) return db_err(conn, r);
    return ok_list(r);
}

static DbResult pg_get_fornecedor_by_id(dp_db_t db, int id) {
    PG_CONN
    char sid[16]; snprintf(sid,sizeof(sid),"%d",id);
    const char *p[]={sid};
    PGresult *r = PQexecParams(conn,"SELECT id,nome,cnpj,contato,email,tel,categoria,prazo,status,criado_em,atualizado_em FROM fornecedores WHERE id=$1",1,NULL,p,NULL,NULL,0);
    if (PQresultStatus(r) != PGRES_TUPLES_OK) return db_err(conn, r);
    return ok_item(r);
}

static DbResult pg_save_fornecedor(dp_db_t db, const char *body) {
    PG_CONN
    cJSON *j = cJSON_Parse(body);
    if (!j) { DbResult r={NULL,strdup("JSON inv\u00e1lido.")}; return r; }
    const char *nome  = cJSON_GetStringValue(cJSON_GetObjectItem(j,"nome"));
    const char *email = cJSON_GetStringValue(cJSON_GetObjectItem(j,"email"));
    if (!nome||!email) { cJSON_Delete(j); DbResult r={NULL,strdup("nome e email s\u00e3o obrigat\u00f3rios.")}; return r; }
    const char *cnpj     = cJSON_GetStringValue(cJSON_GetObjectItem(j,"cnpj"));
    const char *contato  = cJSON_GetStringValue(cJSON_GetObjectItem(j,"contato"));
    const char *tel      = cJSON_GetStringValue(cJSON_GetObjectItem(j,"tel"));
    const char *categoria= cJSON_GetStringValue(cJSON_GetObjectItem(j,"categoria"));
    const char *status_v = cJSON_GetStringValue(cJSON_GetObjectItem(j,"status"));
    char prazo_s[16]="0";
    cJSON *pr=cJSON_GetObjectItem(j,"prazo");
    if (pr) snprintf(prazo_s,sizeof(prazo_s),"%d",(int)pr->valuedouble);
    const char *p[]={nome,cnpj,contato,email,tel,categoria,prazo_s,status_v?status_v:"Ativo"};
    PGresult *r = PQexecParams(conn,
        "INSERT INTO fornecedores(nome,cnpj,contato,email,tel,categoria,prazo,status)"
        " VALUES($1,$2,$3,$4,$5,$6,$7,$8)"
        " RETURNING id,nome,cnpj,contato,email,tel,categoria,prazo,status,criado_em,atualizado_em",
        8,NULL,p,NULL,NULL,0);
    cJSON_Delete(j);
    if (PQresultStatus(r) != PGRES_TUPLES_OK) return db_err(conn, r);
    return ok_item(r);
}

static DbResult pg_update_fornecedor(dp_db_t db, int id, const char *body) {
    PG_CONN
    cJSON *j = cJSON_Parse(body);
    if (!j) { DbResult r={NULL,strdup("JSON inv\u00e1lido.")}; return r; }
    char sid[16]; snprintf(sid,sizeof(sid),"%d",id);
    const char *nome     = cJSON_GetStringValue(cJSON_GetObjectItem(j,"nome"));
    const char *cnpj     = cJSON_GetStringValue(cJSON_GetObjectItem(j,"cnpj"));
    const char *contato  = cJSON_GetStringValue(cJSON_GetObjectItem(j,"contato"));
    const char *email    = cJSON_GetStringValue(cJSON_GetObjectItem(j,"email"));
    const char *tel      = cJSON_GetStringValue(cJSON_GetObjectItem(j,"tel"));
    const char *categoria= cJSON_GetStringValue(cJSON_GetObjectItem(j,"categoria"));
    const char *status_v = cJSON_GetStringValue(cJSON_GetObjectItem(j,"status"));
    char prazo_s[16]="0";
    cJSON *pr=cJSON_GetObjectItem(j,"prazo");
    if (pr) snprintf(prazo_s,sizeof(prazo_s),"%d",(int)pr->valuedouble);
    const char *p[]={sid,nome,cnpj,contato,email,tel,categoria,prazo_s,status_v?status_v:"Ativo"};
    PGresult *r = PQexecParams(conn,
        "UPDATE fornecedores SET nome=$2,cnpj=$3,contato=$4,email=$5,tel=$6,categoria=$7,prazo=$8,status=$9,atualizado_em=NOW() WHERE id=$1"
        " RETURNING id,nome,cnpj,contato,email,tel,categoria,prazo,status,criado_em,atualizado_em",
        9,NULL,p,NULL,NULL,0);
    cJSON_Delete(j);
    if (PQresultStatus(r) != PGRES_TUPLES_OK) return db_err(conn, r);
    return ok_item(r);
}

static DbResult pg_delete_fornecedor(dp_db_t db, int id) {
    PG_CONN
    char sid[16]; snprintf(sid,sizeof(sid),"%d",id);
    const char *p[]={sid};
    PGresult *r = PQexecParams(conn,"DELETE FROM fornecedores WHERE id=$1",1,NULL,p,NULL,NULL,0);
    if (PQresultStatus(r) != PGRES_COMMAND_OK) return db_err(conn, r);
    return ok_deleted(r);
}

/* ═══════════════════════════════════════════════════════════
   PEDIDOS
   ═══════════════════════════════════════════════════════════ */

static DbResult pg_get_pedidos(dp_db_t db) {
    PG_CONN
    PGresult *r = PQexec(conn,
        "SELECT p.id,p.cliente_id,c.nome AS cliente_nome,p.produto_id,pr.nome AS produto_nome,"
        "p.qtd,p.valor,p.destino,p.data_entrega,p.status,p.obs,p.criado_em,p.atualizado_em "
        "FROM pedidos p "
        "LEFT JOIN clientes c ON c.id=p.cliente_id "
        "LEFT JOIN produtos pr ON pr.id=p.produto_id "
        "ORDER BY p.id DESC");
    if (PQresultStatus(r) != PGRES_TUPLES_OK) return db_err(conn, r);
    return ok_list(r);
}

static DbResult pg_get_pedido_by_id(dp_db_t db, int id) {
    PG_CONN
    char sid[16]; snprintf(sid,sizeof(sid),"%d",id);
    const char *p[]={sid};
    PGresult *r = PQexecParams(conn,
        "SELECT p.id,p.cliente_id,c.nome AS cliente_nome,p.produto_id,pr.nome AS produto_nome,"
        "p.qtd,p.valor,p.destino,p.data_entrega,p.status,p.obs,p.criado_em,p.atualizado_em "
        "FROM pedidos p "
        "LEFT JOIN clientes c ON c.id=p.cliente_id "
        "LEFT JOIN produtos pr ON pr.id=p.produto_id "
        "WHERE p.id=$1",1,NULL,p,NULL,NULL,0);
    if (PQresultStatus(r) != PGRES_TUPLES_OK) return db_err(conn, r);
    return ok_item(r);
}

static DbResult pg_save_pedido(dp_db_t db, const char *body) {
    PG_CONN
    cJSON *j = cJSON_Parse(body);
    if (!j) { DbResult r={NULL,strdup("JSON inv\u00e1lido.")}; return r; }
    cJSON *cid=cJSON_GetObjectItem(j,"clienteId"); if(!cid) cid=cJSON_GetObjectItem(j,"cliente_id");
    cJSON *pid=cJSON_GetObjectItem(j,"produtoId");  if(!pid) pid=cJSON_GetObjectItem(j,"produto_id");
    cJSON *qtd=cJSON_GetObjectItem(j,"qtd");
    if (!cid||!pid||!qtd) { cJSON_Delete(j); DbResult r={NULL,strdup("clienteId, produtoId e qtd s\u00e3o obrigat\u00f3rios.")}; return r; }
    char cid_s[16],pid_s[16],qtd_s[16],valor_s[32]="0";
    snprintf(cid_s,sizeof(cid_s),"%d",(int)cid->valuedouble);
    snprintf(pid_s,sizeof(pid_s),"%d",(int)pid->valuedouble);
    snprintf(qtd_s,sizeof(qtd_s),"%d",(int)qtd->valuedouble);
    cJSON *val=cJSON_GetObjectItem(j,"valor");
    if (val) snprintf(valor_s,sizeof(valor_s),"%.2f",val->valuedouble);
    const char *destino=cJSON_GetStringValue(cJSON_GetObjectItem(j,"destino"));
    const char *data=cJSON_GetStringValue(cJSON_GetObjectItem(j,"data"));
    const char *status_v=cJSON_GetStringValue(cJSON_GetObjectItem(j,"status"));
    const char *obs=cJSON_GetStringValue(cJSON_GetObjectItem(j,"obs"));
    cJSON_Delete(j);
    /* Transação: inserir pedido + decrementar estoque */
    PQexec(conn,"BEGIN");
    const char *p[]={cid_s,pid_s,qtd_s,valor_s,destino,data,status_v?status_v:"Pendente",obs};
    PGresult *r = PQexecParams(conn,
        "INSERT INTO pedidos(cliente_id,produto_id,qtd,valor,destino,data_entrega,status,obs)"
        " VALUES($1,$2,$3,$4,$5,$6,$7,$8)"
        " RETURNING id,cliente_id,produto_id,qtd,valor,destino,data_entrega,status,obs,criado_em,atualizado_em",
        8,NULL,p,NULL,NULL,0);
    if (PQresultStatus(r) != PGRES_TUPLES_OK) { PQexec(conn,"ROLLBACK"); return db_err(conn,r); }
    /* Decrementar estoque apenas se não for Cancelado */
    if (!status_v || strcmp(status_v,"Cancelado")!=0) {
        const char *ep[]={qtd_s,pid_s};
        PGresult *eu = PQexecParams(conn,"UPDATE produtos SET estoque=estoque-$1::int WHERE id=$2",2,NULL,ep,NULL,NULL,0);
        if (PQresultStatus(eu) != PGRES_COMMAND_OK) { PQclear(eu); PQclear(r); PQexec(conn,"ROLLBACK"); PGresult *tmp=PQexec(conn,"ROLLBACK"); PQclear(tmp); DbResult er={NULL,strdup("Falha ao atualizar estoque.")}; return er; }
        PQclear(eu);
    }
    PQexec(conn,"COMMIT");
    return ok_item(r);
}

static DbResult pg_update_pedido(dp_db_t db, int id, const char *body) {
    PG_CONN
    cJSON *j = cJSON_Parse(body);
    if (!j) { DbResult r={NULL,strdup("JSON inv\u00e1lido.")}; return r; }
    char sid[16]; snprintf(sid,sizeof(sid),"%d",id);
    const char *destino=cJSON_GetStringValue(cJSON_GetObjectItem(j,"destino"));
    const char *data=cJSON_GetStringValue(cJSON_GetObjectItem(j,"data"));
    const char *status_v=cJSON_GetStringValue(cJSON_GetObjectItem(j,"status"));
    const char *obs=cJSON_GetStringValue(cJSON_GetObjectItem(j,"obs"));
    char qtd_s[16]="0",valor_s[32]="0";
    cJSON *qtd=cJSON_GetObjectItem(j,"qtd"); if(qtd) snprintf(qtd_s,sizeof(qtd_s),"%d",(int)qtd->valuedouble);
    cJSON *val=cJSON_GetObjectItem(j,"valor"); if(val) snprintf(valor_s,sizeof(valor_s),"%.2f",val->valuedouble);
    cJSON_Delete(j);
    const char *p[]={sid,qtd_s,valor_s,destino,data,status_v?status_v:"Pendente",obs};
    PGresult *r = PQexecParams(conn,
        "UPDATE pedidos SET qtd=$2,valor=$3,destino=$4,data_entrega=$5,status=$6,obs=$7,atualizado_em=NOW() WHERE id=$1"
        " RETURNING id,cliente_id,produto_id,qtd,valor,destino,data_entrega,status,obs,criado_em,atualizado_em",
        7,NULL,p,NULL,NULL,0);
    if (PQresultStatus(r) != PGRES_TUPLES_OK) return db_err(conn, r);
    return ok_item(r);
}

static DbResult pg_update_pedido_status(dp_db_t db, int id, const char *status) {
    PG_CONN
    char sid[16]; snprintf(sid,sizeof(sid),"%d",id);
    const char *p[]={sid,status};
    PGresult *r = PQexecParams(conn,
        "UPDATE pedidos SET status=$2,atualizado_em=NOW() WHERE id=$1"
        " RETURNING id,status,atualizado_em",
        2,NULL,p,NULL,NULL,0);
    if (PQresultStatus(r) != PGRES_TUPLES_OK) return db_err(conn, r);
    return ok_item(r);
}

static DbResult pg_delete_pedido(dp_db_t db, int id) {
    PG_CONN
    char sid[16]; snprintf(sid,sizeof(sid),"%d",id);
    /* Repor estoque se pedido não era Cancelado */
    PQexec(conn,"BEGIN");
    const char *sp[]={sid};
    PGresult *sel = PQexecParams(conn,"SELECT qtd,produto_id,status FROM pedidos WHERE id=$1",1,NULL,sp,NULL,NULL,0);
    if (PQresultStatus(sel)==PGRES_TUPLES_OK && PQntuples(sel)>0) {
        const char *st = PQgetvalue(sel,0,2);
        if (strcmp(st,"Cancelado")!=0) {
            const char *ep[]={PQgetvalue(sel,0,0),PQgetvalue(sel,0,1)};
            PGresult *eu = PQexecParams(conn,"UPDATE produtos SET estoque=estoque+$1::int WHERE id=$2",2,NULL,ep,NULL,NULL,0);
            PQclear(eu);
        }
    }
    PQclear(sel);
    PGresult *r = PQexecParams(conn,"DELETE FROM pedidos WHERE id=$1",1,NULL,sp,NULL,NULL,0);
    if (PQresultStatus(r) != PGRES_COMMAND_OK) { PQexec(conn,"ROLLBACK"); return db_err(conn,r); }
    PQexec(conn,"COMMIT");
    return ok_deleted(r);
}

static DbResult pg_get_pedidos_recentes(dp_db_t db) {
    PG_CONN
    PGresult *r = PQexec(conn,
        "SELECT p.id,c.nome AS cliente_nome,pr.nome AS produto_nome,p.qtd,p.valor,p.status,p.criado_em "
        "FROM pedidos p "
        "LEFT JOIN clientes c ON c.id=p.cliente_id "
        "LEFT JOIN produtos pr ON pr.id=p.produto_id "
        "ORDER BY p.criado_em DESC LIMIT 5");
    if (PQresultStatus(r) != PGRES_TUPLES_OK) return db_err(conn, r);
    return ok_list(r);
}

/* ═══════════════════════════════════════════════════════════
   DASHBOARD
   ═══════════════════════════════════════════════════════════ */

static DbResult pg_get_dashboard_kpis(dp_db_t db) {
    PG_CONN
    PGresult *r = PQexec(conn,
        "SELECT "
        "(SELECT COUNT(*) FROM produtos WHERE status='Ativo') AS total_produtos,"
        "(SELECT COUNT(*) FROM clientes WHERE status='Ativo') AS total_clientes,"
        "(SELECT COUNT(*) FROM fornecedores WHERE status='Ativo') AS total_fornecedores,"
        "(SELECT COUNT(*) FROM pedidos WHERE status='Pendente') AS pedidos_pendentes,"
        "(SELECT COUNT(*) FROM pedidos WHERE status='Em Rota') AS pedidos_em_rota,"
        "(SELECT COALESCE(SUM(valor),0) FROM pedidos WHERE status='Entregue') AS faturamento_total,"
        "(SELECT COUNT(*) FROM produtos WHERE estoque<=estoque_min) AS estoque_baixo");
    if (PQresultStatus(r) != PGRES_TUPLES_OK) return db_err(conn, r);
    return ok_item(r);
}

static DbResult pg_get_dashboard_entregas(dp_db_t db, int dias) {
    PG_CONN
    char dias_s[16]; snprintf(dias_s,sizeof(dias_s),"%d",dias>0?dias:7);
    const char *p[]={dias_s};
    PGresult *r = PQexecParams(conn,
        "SELECT DATE(criado_em) AS dia, COUNT(*) AS total "
        "FROM pedidos "
        "WHERE criado_em >= NOW() - ($1::int || ' days')::interval "
        "GROUP BY dia ORDER BY dia ASC",
        1,NULL,p,NULL,NULL,0);
    if (PQresultStatus(r) != PGRES_TUPLES_OK) return db_err(conn, r);
    return ok_list(r);
}

static DbResult pg_get_dashboard_status_pedidos(dp_db_t db) {
    PG_CONN
    PGresult *r = PQexec(conn,"SELECT status, COUNT(*) AS total FROM pedidos GROUP BY status ORDER BY total DESC");
    if (PQresultStatus(r) != PGRES_TUPLES_OK) return db_err(conn, r);
    return ok_list(r);
}

/* ═══════════════════════════════════════════════════════════
   ESTOQUE
   ═══════════════════════════════════════════════════════════ */

static DbResult pg_get_estoque(dp_db_t db) {
    PG_CONN
    PGresult *r = PQexec(conn,"SELECT id,nome,categoria,estoque,estoque_min,status FROM produtos ORDER BY nome");
    if (PQresultStatus(r) != PGRES_TUPLES_OK) return db_err(conn, r);
    return ok_list(r);
}

static DbResult pg_ajustar_estoque(dp_db_t db, int produto_id, int qtd) {
    PG_CONN
    char pid_s[16],qtd_s[16];
    snprintf(pid_s,sizeof(pid_s),"%d",produto_id);
    snprintf(qtd_s,sizeof(qtd_s),"%d",qtd);
    const char *p[]={qtd_s,pid_s};
    PGresult *r = PQexecParams(conn,
        "UPDATE produtos SET estoque=$1,atualizado_em=NOW() WHERE id=$2"
        " RETURNING id,nome,estoque,estoque_min,status",
        2,NULL,p,NULL,NULL,0);
    if (PQresultStatus(r) != PGRES_TUPLES_OK) return db_err(conn, r);
    return ok_item(r);
}

/* ═══════════════════════════════════════════════════════════
   AUTH / USUARIOS
   ═══════════════════════════════════════════════════════════ */

static DbResult pg_get_usuario_by_email(dp_db_t db, const char *email) {
    PG_CONN
    const char *p[]={email};
    PGresult *r = PQexecParams(conn,
        "SELECT id,nome,email,senha_hash,role,criado_em FROM usuarios WHERE email=$1",
        1,NULL,p,NULL,NULL,0);
    if (PQresultStatus(r) != PGRES_TUPLES_OK) return db_err(conn, r);
    return ok_item(r);
}

static DbResult pg_get_usuario_by_id(dp_db_t db, int id) {
    PG_CONN
    char sid[16]; snprintf(sid,sizeof(sid),"%d",id);
    const char *p[]={sid};
    PGresult *r = PQexecParams(conn,
        "SELECT id,nome,email,role,criado_em FROM usuarios WHERE id=$1",
        1,NULL,p,NULL,NULL,0);
    if (PQresultStatus(r) != PGRES_TUPLES_OK) return db_err(conn, r);
    return ok_item(r);
}

static DbResult pg_update_usuario_perfil(dp_db_t db, int id, const char *nome, const char *new_email) {
    PG_CONN
    char sid[16]; snprintf(sid,sizeof(sid),"%d",id);
    const char *p[]={sid,nome,new_email};
    PGresult *r = PQexecParams(conn,
        "UPDATE usuarios SET nome=$2,email=$3,atualizado_em=NOW() WHERE id=$1"
        " RETURNING id,nome,email,role",
        3,NULL,p,NULL,NULL,0);
    if (PQresultStatus(r) != PGRES_TUPLES_OK) return db_err(conn, r);
    return ok_item(r);
}

static DbResult pg_update_usuario_senha(dp_db_t db, int id, const char *senha_hash) {
    PG_CONN
    char sid[16]; snprintf(sid,sizeof(sid),"%d",id);
    const char *p[]={sid,senha_hash};
    PGresult *r = PQexecParams(conn,
        "UPDATE usuarios SET senha_hash=$2,atualizado_em=NOW() WHERE id=$1",
        2,NULL,p,NULL,NULL,0);
    if (PQresultStatus(r) != PGRES_COMMAND_OK) return db_err(conn, r);
    DbResult res = { strdup("{\"success\":true,\"data\":{\"message\":\"Senha atualizada com sucesso.\"}}"), NULL };
    PQclear(r);
    return res;
}

/* ═══════════════════════════════════════════════════════════
   CONFIG EMPRESA
   ═══════════════════════════════════════════════════════════ */

static DbResult pg_get_config(dp_db_t db) {
    PG_CONN
    PGresult *r = PQexec(conn,"SELECT id,razao_social,cnpj,email,tel,endereco,atualizado_em FROM config_empresa LIMIT 1");
    if (PQresultStatus(r) != PGRES_TUPLES_OK) return db_err(conn, r);
    if (PQntuples(r)==0) { PQclear(r); DbResult res={strdup("{\"success\":true,\"data\":null}"),NULL}; return res; }
    return ok_item(r);
}

static DbResult pg_update_config(dp_db_t db, const char *body) {
    PG_CONN
    cJSON *j = cJSON_Parse(body);
    if (!j) { DbResult r={NULL,strdup("JSON inv\u00e1lido.")}; return r; }
    const char *razao   = cJSON_GetStringValue(cJSON_GetObjectItem(j,"razaoSocial")); if(!razao) razao=cJSON_GetStringValue(cJSON_GetObjectItem(j,"razao_social"));
    const char *cnpj    = cJSON_GetStringValue(cJSON_GetObjectItem(j,"cnpj"));
    const char *email   = cJSON_GetStringValue(cJSON_GetObjectItem(j,"email"));
    const char *tel     = cJSON_GetStringValue(cJSON_GetObjectItem(j,"tel"));
    const char *endereco= cJSON_GetStringValue(cJSON_GetObjectItem(j,"endereco"));
    cJSON_Delete(j);
    const char *p[]={razao,cnpj,email,tel,endereco};
    PGresult *r = PQexecParams(conn,
        "INSERT INTO config_empresa(razao_social,cnpj,email,tel,endereco) VALUES($1,$2,$3,$4,$5)"
        " ON CONFLICT (id) DO UPDATE SET razao_social=$1,cnpj=$2,email=$3,tel=$4,endereco=$5,atualizado_em=NOW()"
        " RETURNING id,razao_social,cnpj,email,tel,endereco,atualizado_em",
        5,NULL,p,NULL,NULL,0);
    if (PQresultStatus(r) != PGRES_TUPLES_OK) return db_err(conn, r);
    return ok_item(r);
}

/* ═══════════════════════════════════════════════════════════
   SQLite stub (não implementado)
   ═══════════════════════════════════════════════════════════ */
static DbResult sl_not_implemented(dp_db_t db) {
    (void)db;
    DbResult r={NULL,strdup("SQLite n\u00e3o implementado.")};
    return r;
}

/* ═══════════════════════════════════════════════════════════
   Repository instances
   ═══════════════════════════════════════════════════════════ */

static Repository s_pg_repo = {
    .get_produtos             = pg_get_produtos,
    .get_produto_by_id        = pg_get_produto_by_id,
    .save_produto             = pg_save_produto,
    .update_produto           = pg_update_produto,
    .delete_produto           = pg_delete_produto,
    .get_estoque_baixo        = pg_get_estoque_baixo,
    .get_clientes             = pg_get_clientes,
    .get_cliente_by_id        = pg_get_cliente_by_id,
    .save_cliente             = pg_save_cliente,
    .update_cliente           = pg_update_cliente,
    .delete_cliente           = pg_delete_cliente,
    .get_fornecedores         = pg_get_fornecedores,
    .get_fornecedor_by_id     = pg_get_fornecedor_by_id,
    .save_fornecedor          = pg_save_fornecedor,
    .update_fornecedor        = pg_update_fornecedor,
    .delete_fornecedor        = pg_delete_fornecedor,
    .get_pedidos              = pg_get_pedidos,
    .get_pedido_by_id         = pg_get_pedido_by_id,
    .save_pedido              = pg_save_pedido,
    .update_pedido            = pg_update_pedido,
    .update_pedido_status     = pg_update_pedido_status,
    .delete_pedido            = pg_delete_pedido,
    .get_pedidos_recentes     = pg_get_pedidos_recentes,
    .get_dashboard_kpis       = pg_get_dashboard_kpis,
    .get_dashboard_entregas   = pg_get_dashboard_entregas,
    .get_dashboard_status_pedidos = pg_get_dashboard_status_pedidos,
    .get_estoque              = pg_get_estoque,
    .ajustar_estoque          = pg_ajustar_estoque,
    .get_usuario_by_email     = pg_get_usuario_by_email,
    .get_usuario_by_id        = pg_get_usuario_by_id,
    .update_usuario_perfil    = pg_update_usuario_perfil,
    .update_usuario_senha     = pg_update_usuario_senha,
    .get_config               = pg_get_config,
    .update_config            = pg_update_config,
};

Repository* get_repository(const char *conn_str) {
    if (conn_str && (strstr(conn_str,"postgresql://")||strstr(conn_str,"host=")))
        s_current_repo = &s_pg_repo;
    return s_current_repo;
}

Repository* repo_get_instance(void) {
    return s_current_repo;
}
