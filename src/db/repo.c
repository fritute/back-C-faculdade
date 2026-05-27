#include "distribpro/db.h"
#include "distribpro/repo.h"
#include "cJSON.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <libpq-fe.h>

static Repository *s_current_repo = NULL;

/* â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
   Helpers internos
   â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â• */

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
    DbResult r = { strdup("{\"success\":true,\"data\":{\"message\":\"Registro excluido com sucesso.\"}}"), NULL };
    return r;
}

static DbResult db_err(PGconn *conn, PGresult *res) {
    const char *msg = PQerrorMessage(conn);
    fprintf(stderr, "[DB_ERROR] %s\n", msg);
    DbResult r = { NULL, strdup(msg) };
    if (res) PQclear(res);
    return r;
}

#define PG_CONN  PGconn *conn = (PGconn*)db_get_pg_conn(db); \
                 if (!conn) { DbResult r={NULL,strdup("Conexao DB indisponivel.")}; return r; }

/* Escapa e executa uma query com ate 16 valores literais. */
static PGresult *pg_exec_lit(PGconn *conn, const char *fmt, const char * const vals[], int nvals) {
    char *escaped[16] = {0};
    for (int i = 0; i < nvals && i < 16; i++) {
        if (vals[i] == NULL) {
            escaped[i] = strdup("NULL");
        } else {
            escaped[i] = PQescapeLiteral(conn, vals[i], strlen(vals[i]));
            if (!escaped[i]) escaped[i] = strdup("NULL");
        }
    }
    char buf[8192];
    int bi = 0, vi = 0;
    for (int fi = 0; fmt[fi] && bi < (int)sizeof(buf)-1; fi++) {
        if (fmt[fi] == '?' && vi < nvals) {
            int len = (int)strlen(escaped[vi]);
            if (bi + len < (int)sizeof(buf)-1) {
                memcpy(buf+bi, escaped[vi], len);
                bi += len;
            }
            vi++;
        } else {
            buf[bi++] = fmt[fi];
        }
    }
    buf[bi] = '\0';
    for (int i = 0; i < nvals && i < 16; i++) free(escaped[i]);
    return PQexec(conn, buf);
}

/* â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
   PRODUTOS
   â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â• */

static DbResult pg_get_produtos(dp_db_t db, int fornecedor_id) {
    PG_CONN
    static const char *cols =
        "id,nome,categoria,unidade,preco,estoque,estoque_min,sku,"
        "fornecedor_id,COALESCE(taxa_fornecedor,90) AS taxa_fornecedor,"
        "COALESCE(taxa_operador,10) AS taxa_operador,"
        "status,descricao,img_produtos,criado_em,atualizado_em";
    if (fornecedor_id > 0) {
        char fid_s[16]; snprintf(fid_s, sizeof(fid_s), "%d", fornecedor_id);
        const char *p[] = { fid_s };
        char sql[512];
        snprintf(sql, sizeof(sql),
            "SELECT %s FROM produtos WHERE fornecedor_id=$1 ORDER BY id", cols);
        PGresult *r = PQexecParams(conn, sql, 1, NULL, p, NULL, NULL, 0);
        if (PQresultStatus(r) != PGRES_TUPLES_OK) return db_err(conn, r);
        return ok_list(r);
    }
    char sql[512];
    snprintf(sql, sizeof(sql), "SELECT %s FROM produtos ORDER BY id", cols);
    PGresult *r = PQexec(conn, sql);
    if (PQresultStatus(r) != PGRES_TUPLES_OK) return db_err(conn, r);
    return ok_list(r);
}

static DbResult pg_get_produto_by_id(dp_db_t db, int id) {
    PG_CONN
    char sid[16]; snprintf(sid, sizeof(sid), "%d", id);
    const char *p[] = { sid };
    PGresult *r = pg_exec_lit(conn,
        "SELECT id,nome,categoria,unidade,preco,estoque,estoque_min,sku,"
        "fornecedor_id,COALESCE(taxa_fornecedor,90) AS taxa_fornecedor,"
        "COALESCE(taxa_operador,10) AS taxa_operador,"
        "status,descricao,img_produtos,criado_em,atualizado_em "
        "FROM produtos WHERE id=?", p, 1);
    if (PQresultStatus(r) != PGRES_TUPLES_OK) return db_err(conn, r);
    return ok_item(r);
}

static DbResult pg_save_produto(dp_db_t db, const char *body) {
    PG_CONN
    cJSON *j = cJSON_Parse(body);
    if (!j) { DbResult r={NULL,strdup("JSON invalido.")}; return r; }
    const char *nome      = cJSON_GetStringValue(cJSON_GetObjectItem(j,"nome"));
    const char *categoria = cJSON_GetStringValue(cJSON_GetObjectItem(j,"categoria"));
    if (!nome || !categoria) { cJSON_Delete(j); DbResult r={NULL,strdup("nome e categoria sao obrigatorios.")}; return r; }
    char nome_s[256]="", cat_s[256]="", uni_s[64]="Un", preco_s[32]="0";
    char estoque_s[16]="0", emin_s[16]="10", fid_s[16]="", sku_s[128]="", status_s[64]="Ativo", desc_s[512]="", img_s[512]="";
    char tf_s[16]="90", to_s[16]="10";
    snprintf(nome_s, sizeof(nome_s), "%s", nome);
    snprintf(cat_s, sizeof(cat_s), "%s", categoria);
    const char *unidade  = cJSON_GetStringValue(cJSON_GetObjectItem(j,"unidade"));
    const char *descricao= cJSON_GetStringValue(cJSON_GetObjectItem(j,"descricao"));
    const char *sku      = cJSON_GetStringValue(cJSON_GetObjectItem(j,"sku"));
    const char *status_v = cJSON_GetStringValue(cJSON_GetObjectItem(j,"status"));
    const char *img      = cJSON_GetStringValue(cJSON_GetObjectItem(j,"img_produtos"));
    if (unidade)  snprintf(uni_s,  sizeof(uni_s),  "%s", unidade);
    if (descricao)snprintf(desc_s, sizeof(desc_s), "%s", descricao);
    if (sku)      snprintf(sku_s,  sizeof(sku_s),  "%s", sku);
    if (status_v) snprintf(status_s,sizeof(status_s),"%s",status_v);
    if (img)      snprintf(img_s,  sizeof(img_s),  "%s", img);
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
    cJSON *tf = cJSON_GetObjectItem(j,"taxa_fornecedor");
    if (tf && !cJSON_IsNull(tf)) snprintf(tf_s,sizeof(tf_s),"%.2f",tf->valuedouble);
    cJSON *to = cJSON_GetObjectItem(j,"taxa_operador");
    if (to && !cJSON_IsNull(to)) snprintf(to_s,sizeof(to_s),"%.2f",to->valuedouble);
    cJSON_Delete(j);
    const char *p[] = { nome_s, cat_s, uni_s, preco_s, estoque_s, emin_s,
                        sku_s[0]?sku_s:NULL, fid_p, status_s, desc_s[0]?desc_s:NULL,
                        img_s[0]?img_s:NULL, tf_s, to_s };
    PGresult *r = pg_exec_lit(conn,
        "INSERT INTO produtos(nome,categoria,unidade,preco,estoque,estoque_min,sku,fornecedor_id,status,descricao,img_produtos,taxa_fornecedor,taxa_operador)"
        " VALUES(?,?,?,?::numeric,?::int,?::int,?,?::int,?,?,?,?::numeric,?::numeric)"
        " RETURNING id,nome,categoria,unidade,preco,estoque,estoque_min,sku,fornecedor_id,"
        "COALESCE(taxa_fornecedor,90) AS taxa_fornecedor,COALESCE(taxa_operador,10) AS taxa_operador,"
        "status,descricao,img_produtos,criado_em,atualizado_em",
        p, 13);
    if (PQresultStatus(r) != PGRES_TUPLES_OK) return db_err(conn, r);
    return ok_item(r);
}

static DbResult pg_update_produto(dp_db_t db, int id, const char *body) {
    PG_CONN
    cJSON *j = cJSON_Parse(body);
    if (!j) { DbResult r={NULL,strdup("JSON invalido.")}; return r; }
    char sid[16]; snprintf(sid,sizeof(sid),"%d",id);
    char nome_s[256]="", cat_s[256]="", uni_s[64]="Un", preco_s[32]="0";
    char estoque_s[16]="0", emin_s[16]="10", fid_s[16]="", sku_s[128]="", status_s[64]="Ativo", desc_s[512]="", img_s[512]="";
    char tf_s[16]="90", to_s[16]="10";
    const char *nome      = cJSON_GetStringValue(cJSON_GetObjectItem(j,"nome"));
    const char *categoria = cJSON_GetStringValue(cJSON_GetObjectItem(j,"categoria"));
    const char *unidade   = cJSON_GetStringValue(cJSON_GetObjectItem(j,"unidade"));
    const char *descricao = cJSON_GetStringValue(cJSON_GetObjectItem(j,"descricao"));
    const char *sku       = cJSON_GetStringValue(cJSON_GetObjectItem(j,"sku"));
    const char *status_v  = cJSON_GetStringValue(cJSON_GetObjectItem(j,"status"));
    const char *img       = cJSON_GetStringValue(cJSON_GetObjectItem(j,"img_produtos"));
    if (nome)     snprintf(nome_s,  sizeof(nome_s),  "%s", nome);
    if (categoria)snprintf(cat_s,   sizeof(cat_s),   "%s", categoria);
    if (unidade)  snprintf(uni_s,   sizeof(uni_s),   "%s", unidade);
    if (descricao)snprintf(desc_s,  sizeof(desc_s),  "%s", descricao);
    if (sku)      snprintf(sku_s,   sizeof(sku_s),   "%s", sku);
    if (status_v) snprintf(status_s,sizeof(status_s),"%s", status_v);
    if (img)      snprintf(img_s,   sizeof(img_s),   "%s", img);
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
    cJSON *tf = cJSON_GetObjectItem(j,"taxa_fornecedor");
    if (tf && !cJSON_IsNull(tf)) snprintf(tf_s,sizeof(tf_s),"%.2f",tf->valuedouble);
    cJSON *to = cJSON_GetObjectItem(j,"taxa_operador");
    if (to && !cJSON_IsNull(to)) snprintf(to_s,sizeof(to_s),"%.2f",to->valuedouble);
    cJSON_Delete(j);
    const char *p[] = { nome_s, cat_s, uni_s, preco_s, estoque_s, emin_s,
                        sku_s[0]?sku_s:NULL, fid_p, status_s, desc_s[0]?desc_s:NULL,
                        img_s[0]?img_s:NULL, tf_s, to_s, sid };
    PGresult *r = pg_exec_lit(conn,
        "UPDATE produtos SET nome=?,categoria=?,unidade=?,preco=?::numeric,estoque=?::int,estoque_min=?::int,"
        "sku=?,fornecedor_id=?::int,status=?,descricao=?,img_produtos=?,"
        "taxa_fornecedor=?::numeric,taxa_operador=?::numeric,atualizado_em=NOW() WHERE id=?::int"
        " RETURNING id,nome,categoria,unidade,preco,estoque,estoque_min,sku,fornecedor_id,"
        "COALESCE(taxa_fornecedor,90) AS taxa_fornecedor,COALESCE(taxa_operador,10) AS taxa_operador,"
        "status,descricao,img_produtos,criado_em,atualizado_em",
        p, 14);
    if (PQresultStatus(r) != PGRES_TUPLES_OK) return db_err(conn, r);
    return ok_item(r);
}

static DbResult pg_update_produto_imagem(dp_db_t db, int id, const char *img_url) {
    PG_CONN
    char sid[16]; snprintf(sid, sizeof(sid), "%d", id);
    const char *p[] = { img_url, sid };
    PGresult *r = pg_exec_lit(conn,
        "UPDATE produtos SET img_produtos=?,atualizado_em=NOW() WHERE id=?::int"
        " RETURNING id,nome,img_produtos",
        p, 2);
    if (PQresultStatus(r) != PGRES_TUPLES_OK) return db_err(conn, r);
    if (PQntuples(r) == 0) { PQclear(r); DbResult rr={NULL,strdup("NOT_FOUND")}; return rr; }
    return ok_item(r);
}

static DbResult pg_delete_produto(dp_db_t db, int id) {
    PG_CONN
    char sid[16]; snprintf(sid,sizeof(sid),"%d",id);
    const char *p[] = { sid };
    PGresult *r = pg_exec_lit(conn,"DELETE FROM produtos WHERE id=?::int", p, 1);
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

/* â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
   CLIENTES
   â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â• */

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
    PGresult *r = pg_exec_lit(conn,
        "SELECT id,nome,tipo,doc,email,tel,cidade,estado,limite,status,criado_em,atualizado_em"
        " FROM clientes WHERE id=?::int", p, 1);
    if (PQresultStatus(r) != PGRES_TUPLES_OK) return db_err(conn, r);
    return ok_item(r);
}

static DbResult pg_save_cliente(dp_db_t db, const char *body) {
    PG_CONN
    cJSON *j = cJSON_Parse(body);
    if (!j) { DbResult r={NULL,strdup("JSON invalido.")}; return r; }
    const char *nome   = cJSON_GetStringValue(cJSON_GetObjectItem(j,"nome"));
    const char *email  = cJSON_GetStringValue(cJSON_GetObjectItem(j,"email"));
    if (!nome||!email) { cJSON_Delete(j); DbResult r={NULL,strdup("nome e email sao obrigatorios.")}; return r; }
    char nome_s[256]="",email_s[256]="",tipo_s[32]="PJ",doc_s[64]="",tel_s[64]="";
    char cidade_s[128]="",estado_s[8]="",limite_s[32]="0",status_s[64]="Ativo";
    snprintf(nome_s, sizeof(nome_s), "%s", nome);
    snprintf(email_s,sizeof(email_s),"%s", email);
    const char *tipo   = cJSON_GetStringValue(cJSON_GetObjectItem(j,"tipo"));
    const char *doc    = cJSON_GetStringValue(cJSON_GetObjectItem(j,"doc"));
    const char *tel    = cJSON_GetStringValue(cJSON_GetObjectItem(j,"tel"));
    const char *cidade = cJSON_GetStringValue(cJSON_GetObjectItem(j,"cidade"));
    const char *estado = cJSON_GetStringValue(cJSON_GetObjectItem(j,"estado"));
    const char *status_v = cJSON_GetStringValue(cJSON_GetObjectItem(j,"status"));
    if (tipo)    snprintf(tipo_s,  sizeof(tipo_s),  "%s", tipo);
    if (doc)     snprintf(doc_s,   sizeof(doc_s),   "%s", doc);
    if (tel)     snprintf(tel_s,   sizeof(tel_s),   "%s", tel);
    if (cidade)  snprintf(cidade_s,sizeof(cidade_s),"%s", cidade);
    if (estado)  snprintf(estado_s,sizeof(estado_s),"%s", estado);
    if (status_v)snprintf(status_s,sizeof(status_s),"%s", status_v);
    cJSON *lim = cJSON_GetObjectItem(j,"limite");
    if (lim) snprintf(limite_s,sizeof(limite_s),"%.2f",lim->valuedouble);
    cJSON_Delete(j);
    const char *p[]={nome_s,tipo_s,doc_s,email_s,tel_s,cidade_s,estado_s,limite_s,status_s};
    PGresult *r = pg_exec_lit(conn,
        "INSERT INTO clientes(nome,tipo,doc,email,tel,cidade,estado,limite,status)"
        " VALUES(?,?,?,?,?,?,?,?::numeric,?)"
        " RETURNING id,nome,tipo,doc,email,tel,cidade,estado,limite,status,criado_em,atualizado_em",
        p, 9);
    if (PQresultStatus(r) != PGRES_TUPLES_OK) return db_err(conn, r);
    return ok_item(r);
}

static DbResult pg_update_cliente(dp_db_t db, int id, const char *body) {
    PG_CONN
    cJSON *j = cJSON_Parse(body);
    if (!j) { DbResult r={NULL,strdup("JSON invalido.")}; return r; }
    char sid[16]; snprintf(sid,sizeof(sid),"%d",id);
    char nome_s[256]="",email_s[256]="",tipo_s[32]="PJ",doc_s[64]="",tel_s[64]="";
    char cidade_s[128]="",estado_s[8]="",limite_s[32]="0",status_s[64]="Ativo";
    const char *nome   = cJSON_GetStringValue(cJSON_GetObjectItem(j,"nome"));
    const char *email  = cJSON_GetStringValue(cJSON_GetObjectItem(j,"email"));
    const char *tipo   = cJSON_GetStringValue(cJSON_GetObjectItem(j,"tipo"));
    const char *doc    = cJSON_GetStringValue(cJSON_GetObjectItem(j,"doc"));
    const char *tel    = cJSON_GetStringValue(cJSON_GetObjectItem(j,"tel"));
    const char *cidade = cJSON_GetStringValue(cJSON_GetObjectItem(j,"cidade"));
    const char *estado = cJSON_GetStringValue(cJSON_GetObjectItem(j,"estado"));
    const char *status_v = cJSON_GetStringValue(cJSON_GetObjectItem(j,"status"));
    if (nome)    snprintf(nome_s,  sizeof(nome_s),  "%s", nome);
    if (email)   snprintf(email_s, sizeof(email_s), "%s", email);
    if (tipo)    snprintf(tipo_s,  sizeof(tipo_s),  "%s", tipo);
    if (doc)     snprintf(doc_s,   sizeof(doc_s),   "%s", doc);
    if (tel)     snprintf(tel_s,   sizeof(tel_s),   "%s", tel);
    if (cidade)  snprintf(cidade_s,sizeof(cidade_s),"%s", cidade);
    if (estado)  snprintf(estado_s,sizeof(estado_s),"%s", estado);
    if (status_v)snprintf(status_s,sizeof(status_s),"%s", status_v);
    cJSON *lim=cJSON_GetObjectItem(j,"limite");
    if (lim) snprintf(limite_s,sizeof(limite_s),"%.2f",lim->valuedouble);
    cJSON_Delete(j);
    const char *p[]={nome_s,tipo_s,doc_s,email_s,tel_s,cidade_s,estado_s,limite_s,status_s,sid};
    PGresult *r = pg_exec_lit(conn,
        "UPDATE clientes SET nome=?,tipo=?,doc=?,email=?,tel=?,cidade=?,estado=?,limite=?::numeric,status=?,atualizado_em=NOW() WHERE id=?::int"
        " RETURNING id,nome,tipo,doc,email,tel,cidade,estado,limite,status,criado_em,atualizado_em",
        p, 10);
    if (PQresultStatus(r) != PGRES_TUPLES_OK) return db_err(conn, r);
    return ok_item(r);
}

static DbResult pg_delete_cliente(dp_db_t db, int id) {
    PG_CONN
    char sid[16]; snprintf(sid,sizeof(sid),"%d",id);
    const char *p[]={sid};
    PGresult *r = pg_exec_lit(conn,"DELETE FROM clientes WHERE id=?::int", p, 1);
    if (PQresultStatus(r) != PGRES_COMMAND_OK) return db_err(conn, r);
    return ok_deleted(r);
}

/* â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
   FORNECEDORES
   â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â• */

static DbResult pg_get_fornecedores(dp_db_t db) {
    PG_CONN
    PGresult *r = PQexec(conn,"SELECT id,nome,'' AS cnpj,'' AS contato,email,'' AS tel,'' AS categoria,0 AS prazo,'Ativo' AS status,criado_em,atualizado_em FROM usuarios WHERE role='admin' ORDER BY id");
    if (PQresultStatus(r) != PGRES_TUPLES_OK) return db_err(conn, r);
    return ok_list(r);
}

static DbResult pg_get_fornecedor_by_id(dp_db_t db, int id) {
    PG_CONN
    char sid[16]; snprintf(sid,sizeof(sid),"%d",id);
    const char *p[]={sid};
    PGresult *r = pg_exec_lit(conn,
        "SELECT id,nome,'' AS cnpj,'' AS contato,email,'' AS tel,'' AS categoria,0 AS prazo,'Ativo' AS status,criado_em,atualizado_em"
        " FROM usuarios WHERE id=?::int AND role='admin'", p, 1);
    if (PQresultStatus(r) != PGRES_TUPLES_OK) return db_err(conn, r);
    return ok_item(r);
}

static DbResult pg_save_fornecedor(dp_db_t db, const char *body) {
    PG_CONN
    cJSON *j = cJSON_Parse(body);
    if (!j) { DbResult r={NULL,strdup("JSON invalido.")}; return r; }
    const char *nome  = cJSON_GetStringValue(cJSON_GetObjectItem(j,"nome"));
    const char *email = cJSON_GetStringValue(cJSON_GetObjectItem(j,"email"));
    if (!nome||!email) { cJSON_Delete(j); DbResult r={NULL,strdup("nome e email sao obrigatorios.")}; return r; }
    char nome_s[256]="",email_s[256]="",cnpj_s[32]="",contato_s[128]="";
    char tel_s[64]="",cat_s[128]="",prazo_s[16]="0",status_s[64]="Ativo";
    snprintf(nome_s, sizeof(nome_s), "%s", nome);
    snprintf(email_s,sizeof(email_s),"%s", email);
    const char *cnpj     = cJSON_GetStringValue(cJSON_GetObjectItem(j,"cnpj"));
    const char *contato  = cJSON_GetStringValue(cJSON_GetObjectItem(j,"contato"));
    const char *tel      = cJSON_GetStringValue(cJSON_GetObjectItem(j,"tel"));
    const char *categoria= cJSON_GetStringValue(cJSON_GetObjectItem(j,"categoria"));
    const char *status_v = cJSON_GetStringValue(cJSON_GetObjectItem(j,"status"));
    if (cnpj)    snprintf(cnpj_s,   sizeof(cnpj_s),   "%s", cnpj);
    if (contato) snprintf(contato_s,sizeof(contato_s), "%s", contato);
    if (tel)     snprintf(tel_s,    sizeof(tel_s),     "%s", tel);
    if (categoria)snprintf(cat_s,   sizeof(cat_s),     "%s", categoria);
    if (status_v)snprintf(status_s, sizeof(status_s),  "%s", status_v);
    cJSON *pr=cJSON_GetObjectItem(j,"prazo");
    if (pr) snprintf(prazo_s,sizeof(prazo_s),"%d",(int)pr->valuedouble);
    cJSON_Delete(j);
    const char *p[]={nome_s,cnpj_s,contato_s,email_s,tel_s,cat_s,prazo_s,status_s};
    PGresult *r = pg_exec_lit(conn,
        "INSERT INTO fornecedores(nome,cnpj,contato,email,tel,categoria,prazo,status)"
        " VALUES(?,?,?,?,?,?,?::int,?)"
        " RETURNING id,nome,cnpj,contato,email,tel,categoria,prazo,status,criado_em,atualizado_em",
        p, 8);
    if (PQresultStatus(r) != PGRES_TUPLES_OK) return db_err(conn, r);
    return ok_item(r);
}

static DbResult pg_update_fornecedor(dp_db_t db, int id, const char *body) {
    PG_CONN
    cJSON *j = cJSON_Parse(body);
    if (!j) { DbResult r={NULL,strdup("JSON invalido.")}; return r; }
    char sid[16]; snprintf(sid,sizeof(sid),"%d",id);
    char nome_s[256]="",email_s[256]="",cnpj_s[32]="",contato_s[128]="";
    char tel_s[64]="",cat_s[128]="",prazo_s[16]="0",status_s[64]="Ativo";
    const char *nome     = cJSON_GetStringValue(cJSON_GetObjectItem(j,"nome"));
    const char *cnpj     = cJSON_GetStringValue(cJSON_GetObjectItem(j,"cnpj"));
    const char *contato  = cJSON_GetStringValue(cJSON_GetObjectItem(j,"contato"));
    const char *email    = cJSON_GetStringValue(cJSON_GetObjectItem(j,"email"));
    const char *tel      = cJSON_GetStringValue(cJSON_GetObjectItem(j,"tel"));
    const char *categoria= cJSON_GetStringValue(cJSON_GetObjectItem(j,"categoria"));
    const char *status_v = cJSON_GetStringValue(cJSON_GetObjectItem(j,"status"));
    if (nome)    snprintf(nome_s,   sizeof(nome_s),    "%s", nome);
    if (cnpj)    snprintf(cnpj_s,   sizeof(cnpj_s),    "%s", cnpj);
    if (contato) snprintf(contato_s,sizeof(contato_s), "%s", contato);
    if (email)   snprintf(email_s,  sizeof(email_s),   "%s", email);
    if (tel)     snprintf(tel_s,    sizeof(tel_s),     "%s", tel);
    if (categoria)snprintf(cat_s,   sizeof(cat_s),     "%s", categoria);
    if (status_v)snprintf(status_s, sizeof(status_s),  "%s", status_v);
    cJSON *pr=cJSON_GetObjectItem(j,"prazo");
    if (pr) snprintf(prazo_s,sizeof(prazo_s),"%d",(int)pr->valuedouble);
    cJSON_Delete(j);
    const char *p[]={nome_s,cnpj_s,contato_s,email_s,tel_s,cat_s,prazo_s,status_s,sid};
    PGresult *r = pg_exec_lit(conn,
        "UPDATE fornecedores SET nome=?,cnpj=?,contato=?,email=?,tel=?,categoria=?,prazo=?::int,status=?,atualizado_em=NOW() WHERE id=?::int"
        " RETURNING id,nome,cnpj,contato,email,tel,categoria,prazo,status,criado_em,atualizado_em",
        p, 9);
    if (PQresultStatus(r) != PGRES_TUPLES_OK) return db_err(conn, r);
    return ok_item(r);
}

static DbResult pg_delete_fornecedor(dp_db_t db, int id) {
    PG_CONN
    char sid[16]; snprintf(sid,sizeof(sid),"%d",id);
    const char *p[]={sid};
    PGresult *r = pg_exec_lit(conn,"DELETE FROM fornecedores WHERE id=?::int", p, 1);
    if (PQresultStatus(r) != PGRES_COMMAND_OK) return db_err(conn, r);
    return ok_deleted(r);
}

/* â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
   PEDIDOS (com itens_pedido)
   â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â• */

/* Helper: busca itens de um pedido e retorna cJSON array */
static cJSON *fetch_itens_pedido(PGconn *conn, const char *pedido_id) {
    const char *p[] = { pedido_id };
    PGresult *r = PQexecParams(conn,
        "SELECT ip.id,ip.produto_id,pr.nome AS produto_nome,ip.qtd,ip.preco_unit,ip.subtotal "
        "FROM itens_pedido ip "
        "LEFT JOIN produtos pr ON pr.id=ip.produto_id "
        "WHERE ip.pedido_id=$1 ORDER BY ip.id",
        1, NULL, p, NULL, NULL, 0);
    if (PQresultStatus(r) != PGRES_TUPLES_OK) { PQclear(r); return cJSON_CreateArray(); }
    cJSON *arr = pg_to_array(r);
    PQclear(r);
    return arr;
}

/* Helper: retorna pedido + itens embutidos */
static DbResult ok_pedido_com_itens(PGconn *conn, PGresult *res) {
    if (PQntuples(res) == 0) { PQclear(res); DbResult r = { NULL, strdup("NOT_FOUND") }; return r; }
    cJSON *root = cJSON_CreateObject();
    cJSON_AddBoolToObject(root, "success", 1);
    cJSON *arr = pg_to_array(res);
    cJSON *item = cJSON_DetachItemFromArray(arr, 0);
    cJSON_Delete(arr);
    char pid_s[16];
    snprintf(pid_s, sizeof(pid_s), "%d", (int)cJSON_GetNumberValue(cJSON_GetObjectItem(item, "id")));
    cJSON_AddItemToObject(item, "itens", fetch_itens_pedido(conn, pid_s));
    cJSON_AddItemToObject(root, "data", item);
    DbResult r = { cJSON_PrintUnformatted(root), NULL };
    cJSON_Delete(root); PQclear(res);
    return r;
}

/* Helper: retorna lista de pedidos, cada um com seus itens */
static DbResult ok_pedidos_com_itens(PGconn *conn, PGresult *res) {
    cJSON *root = cJSON_CreateObject();
    cJSON_AddBoolToObject(root, "success", 1);
    cJSON *data_arr = cJSON_CreateArray();
    int rows = PQntuples(res);
    int cols = PQnfields(res);
    for (int i = 0; i < rows; i++) {
        cJSON *obj = cJSON_CreateObject();
        for (int col = 0; col < cols; col++) {
            const char *key = PQfname(res, col);
            if (PQgetisnull(res, i, col)) { cJSON_AddNullToObject(obj, key); continue; }
            const char *v = PQgetvalue(res, i, col);
            Oid t = PQftype(res, col);
            if (t==20||t==21||t==23||t==700||t==701||t==1700)
                cJSON_AddNumberToObject(obj, key, atof(v));
            else if (t == 16)
                cJSON_AddBoolToObject(obj, key, v[0]=='t');
            else
                cJSON_AddStringToObject(obj, key, v);
        }
        char pid_s[16];
        snprintf(pid_s, sizeof(pid_s), "%s", PQgetvalue(res, i, 0));
        cJSON_AddItemToObject(obj, "itens", fetch_itens_pedido(conn, pid_s));
        cJSON_AddItemToArray(data_arr, obj);
    }
    cJSON_AddItemToObject(root, "data", data_arr);
    DbResult r = { cJSON_PrintUnformatted(root), NULL };
    cJSON_Delete(root); PQclear(res);
    return r;
}

static DbResult pg_get_pedidos(dp_db_t db) {
    PG_CONN
    PGresult *r = PQexec(conn,
        "SELECT p.id,p.cliente_id,c.nome AS cliente_nome,"
        "p.fornecedor_id,f.nome AS fornecedor_nome,"
        "p.valor,COALESCE(p.taxa_fornecedor,90) AS taxa_fornecedor,"
        "COALESCE(p.taxa_operador,10) AS taxa_operador,"
        "p.destino,p.data_entrega,p.status,"
        "COALESCE(p.status_pagamento,'Pendente') AS status_pagamento,"
        "p.obs,p.criado_em,p.atualizado_em "
        "FROM pedidos p "
        "LEFT JOIN clientes c ON c.id=p.cliente_id "
        "LEFT JOIN fornecedores f ON f.id=p.fornecedor_id "
        "ORDER BY p.id DESC");
    if (PQresultStatus(r) != PGRES_TUPLES_OK) return db_err(conn, r);
    return ok_pedidos_com_itens(conn, r);
}

static DbResult pg_get_pedido_by_id(dp_db_t db, int id) {
    PG_CONN
    char sid[16]; snprintf(sid,sizeof(sid),"%d",id);
    const char *p[]={sid};
    PGresult *r = PQexecParams(conn,
        "SELECT p.id,p.cliente_id,c.nome AS cliente_nome,"
        "p.fornecedor_id,f.nome AS fornecedor_nome,"
        "p.valor,COALESCE(p.taxa_fornecedor,90) AS taxa_fornecedor,"
        "COALESCE(p.taxa_operador,10) AS taxa_operador,"
        "p.destino,p.data_entrega,p.status,"
        "COALESCE(p.status_pagamento,'Pendente') AS status_pagamento,"
        "p.obs,p.criado_em,p.atualizado_em "
        "FROM pedidos p "
        "LEFT JOIN clientes c ON c.id=p.cliente_id "
        "LEFT JOIN fornecedores f ON f.id=p.fornecedor_id "
        "WHERE p.id=$1",1,NULL,p,NULL,NULL,0);
    if (PQresultStatus(r) != PGRES_TUPLES_OK) return db_err(conn, r);
    return ok_pedido_com_itens(conn, r);
}

static DbResult pg_save_pedido(dp_db_t db, const char *body) {
    PG_CONN
    cJSON *j = cJSON_Parse(body);
    if (!j) { DbResult r={NULL,strdup("JSON invalido.")}; return r; }

    /* cliente_id */
    cJSON *cid = cJSON_GetObjectItem(j,"cliente_id");
    if (!cid) cid = cJSON_GetObjectItem(j,"clienteId");
    if (!cid) { cJSON_Delete(j); DbResult r={NULL,strdup("cliente_id e obrigatorio.")}; return r; }
    char cid_s[16]; snprintf(cid_s,sizeof(cid_s),"%d",(int)cid->valuedouble);

    /* fornecedor_id */
    char fid_s[16]=""; const char *fid_p = NULL;
    cJSON *fid = cJSON_GetObjectItem(j,"fornecedor_id");
    if (fid && !cJSON_IsNull(fid)) { snprintf(fid_s,sizeof(fid_s),"%d",(int)fid->valuedouble); fid_p=fid_s; }

    /* valor_total (aceita valor_total, valorTotal ou valor) */
    char valor_s[32]="0";
    cJSON *val = cJSON_GetObjectItem(j,"valor_total");
    if (!val) val = cJSON_GetObjectItem(j,"valorTotal");
    if (!val) val = cJSON_GetObjectItem(j,"valor");
    if (val) snprintf(valor_s,sizeof(valor_s),"%.2f",val->valuedouble);

    /* outros campos do cabeÃ§alho */
    const char *destino  = cJSON_GetStringValue(cJSON_GetObjectItem(j,"destino"));
    const char *data     = cJSON_GetStringValue(cJSON_GetObjectItem(j,"data_entrega"));
    if (!data) data      = cJSON_GetStringValue(cJSON_GetObjectItem(j,"data"));
    const char *status_v = cJSON_GetStringValue(cJSON_GetObjectItem(j,"status"));
    if (!status_v) status_v = "Pendente";
    const char *obs      = cJSON_GetStringValue(cJSON_GetObjectItem(j,"observacoes"));
    if (!obs) obs        = cJSON_GetStringValue(cJSON_GetObjectItem(j,"obs"));
    const char *sp       = cJSON_GetStringValue(cJSON_GetObjectItem(j,"status_pagamento"));
    if (!sp) sp          = "Pendente";

    /* taxas: lÃª do cabeÃ§alho ou do primeiro item de taxas_aplicadas */
    char tf_s[16]="90", to_s[16]="10";
    cJSON *tf_top = cJSON_GetObjectItem(j,"taxa_fornecedor");
    cJSON *to_top = cJSON_GetObjectItem(j,"taxa_operador");
    if (tf_top && !cJSON_IsNull(tf_top)) snprintf(tf_s,sizeof(tf_s),"%.2f",tf_top->valuedouble);
    if (to_top && !cJSON_IsNull(to_top)) snprintf(to_s,sizeof(to_s),"%.2f",to_top->valuedouble);
    cJSON *taxas_arr = cJSON_GetObjectItem(j,"taxas_aplicadas");
    if (taxas_arr && cJSON_IsArray(taxas_arr) && cJSON_GetArraySize(taxas_arr) > 0) {
        cJSON *first = cJSON_GetArrayItem(taxas_arr, 0);
        cJSON *tf2 = cJSON_GetObjectItem(first,"taxa_fornecedor");
        cJSON *to2 = cJSON_GetObjectItem(first,"taxa_operador");
        if (tf2 && !cJSON_IsNull(tf2)) snprintf(tf_s,sizeof(tf_s),"%.2f",tf2->valuedouble);
        if (to2 && !cJSON_IsNull(to2)) snprintf(to_s,sizeof(to_s),"%.2f",to2->valuedouble);
    }

    /* ComeÃ§a transaÃ§Ã£o */
    PQexec(conn,"BEGIN");

    const char *hp[] = {cid_s,fid_p,valor_s,destino,data,status_v,obs,sp,tf_s,to_s};
    PGresult *r = PQexecParams(conn,
        "INSERT INTO pedidos(cliente_id,fornecedor_id,valor,destino,data_entrega,status,obs,status_pagamento,taxa_fornecedor,taxa_operador)"
        " VALUES($1,$2::int,$3::numeric,$4,$5,$6,$7,$8,$9::numeric,$10::numeric)"
        " RETURNING id",
        10,NULL,hp,NULL,NULL,0);
    if (PQresultStatus(r) != PGRES_TUPLES_OK) {
        PQexec(conn,"ROLLBACK"); cJSON_Delete(j); return db_err(conn,r);
    }
    int pedido_id = atoi(PQgetvalue(r,0,0));
    char pedido_id_s[16]; snprintf(pedido_id_s,sizeof(pedido_id_s),"%d",pedido_id);
    PQclear(r);

    /* Insere itens a partir do array produtos[] */
    cJSON *produtos_arr = cJSON_GetObjectItem(j,"produtos");
    int inserted_items = 0;
    if (produtos_arr && cJSON_IsArray(produtos_arr)) {
        int np = cJSON_GetArraySize(produtos_arr);
        for (int i = 0; i < np; i++) {
            cJSON *item = cJSON_GetArrayItem(produtos_arr, i);
            cJSON *pid_j = cJSON_GetObjectItem(item,"produto_id");
            cJSON *qtd_j = cJSON_GetObjectItem(item,"quantidade");
            if (!qtd_j) qtd_j = cJSON_GetObjectItem(item,"qtd");
            cJSON *pu_j  = cJSON_GetObjectItem(item,"preco_unitario");
            if (!pu_j) pu_j = cJSON_GetObjectItem(item,"preco_unit");
            if (!pid_j || !qtd_j) continue;
            char pid_s[16], qtd_s[16], pu_s[32], sub_s[32];
            snprintf(pid_s,sizeof(pid_s),"%d",(int)pid_j->valuedouble);
            int qtd_int = (int)qtd_j->valuedouble;
            snprintf(qtd_s,sizeof(qtd_s),"%d",qtd_int);
            double pu = pu_j ? pu_j->valuedouble : 0.0;
            snprintf(pu_s, sizeof(pu_s),"%.2f",pu);
            snprintf(sub_s,sizeof(sub_s),"%.2f",pu * qtd_int);
            const char *ip[]={pedido_id_s,pid_s,qtd_s,pu_s,sub_s};
            PGresult *ri = PQexecParams(conn,
                "INSERT INTO itens_pedido(pedido_id,produto_id,qtd,preco_unit,subtotal)"
                " VALUES($1,$2,$3,$4,$5)",
                5,NULL,ip,NULL,NULL,0);
            if (PQresultStatus(ri) != PGRES_COMMAND_OK) {
                PQclear(ri); PQexec(conn,"ROLLBACK"); cJSON_Delete(j);
                DbResult er={NULL,strdup("Falha ao inserir itens do pedido.")}; return er;
            }
            PQclear(ri);
            /* Desconta estoque (exceto pedidos Cancelados) */
            if (strcmp(status_v,"Cancelado") != 0) {
                const char *ep[]={qtd_s,pid_s};
                PGresult *eu = PQexecParams(conn,
                    "UPDATE produtos SET estoque=estoque-$1::int WHERE id=$2",
                    2,NULL,ep,NULL,NULL,0);
                PQclear(eu);
            }
            inserted_items++;
        }
    }

    /* Fallback: payload antigo com produto_id/qtd direto no body */
    if (inserted_items == 0) {
        cJSON *pid_old = cJSON_GetObjectItem(j,"produto_id");
        if (!pid_old) pid_old = cJSON_GetObjectItem(j,"produtoId");
        cJSON *qtd_old = cJSON_GetObjectItem(j,"qtd");
        if (pid_old && qtd_old) {
            char pid_s[16], qtd_s[16], pu_s[32], sub_s[32];
            snprintf(pid_s,sizeof(pid_s),"%d",(int)pid_old->valuedouble);
            int qtd_int = (int)qtd_old->valuedouble;
            snprintf(qtd_s,sizeof(qtd_s),"%d",qtd_int);
            double pu = qtd_int > 0 && val ? val->valuedouble / qtd_int : 0.0;
            snprintf(pu_s, sizeof(pu_s),"%.2f",pu);
            snprintf(sub_s,sizeof(sub_s),"%.2f",val ? val->valuedouble : 0.0);
            const char *ip[]={pedido_id_s,pid_s,qtd_s,pu_s,sub_s};
            PGresult *ri = PQexecParams(conn,
                "INSERT INTO itens_pedido(pedido_id,produto_id,qtd,preco_unit,subtotal)"
                " VALUES($1,$2,$3,$4,$5)",
                5,NULL,ip,NULL,NULL,0);
            if (PQresultStatus(ri) != PGRES_COMMAND_OK) {
                PQclear(ri); PQexec(conn,"ROLLBACK"); cJSON_Delete(j);
                DbResult er={NULL,strdup("Falha ao inserir item do pedido.")}; return er;
            }
            PQclear(ri);
            if (strcmp(status_v,"Cancelado") != 0) {
                const char *ep[]={qtd_s,pid_s};
                PGresult *eu = PQexecParams(conn,
                    "UPDATE produtos SET estoque=estoque-$1::int WHERE id=$2",
                    2,NULL,ep,NULL,NULL,0);
                PQclear(eu);
            }
        }
    }

    cJSON_Delete(j);
    PQexec(conn,"COMMIT");
    return pg_get_pedido_by_id(db, pedido_id);
}

static DbResult pg_update_pedido(dp_db_t db, int id, const char *body) {
    PG_CONN
    cJSON *j = cJSON_Parse(body);
    if (!j) { DbResult r={NULL,strdup("JSON invalido.")}; return r; }
    char sid[16]; snprintf(sid,sizeof(sid),"%d",id);
    const char *destino  = cJSON_GetStringValue(cJSON_GetObjectItem(j,"destino"));
    const char *data     = cJSON_GetStringValue(cJSON_GetObjectItem(j,"data_entrega"));
    if (!data) data      = cJSON_GetStringValue(cJSON_GetObjectItem(j,"data"));
    const char *status_v = cJSON_GetStringValue(cJSON_GetObjectItem(j,"status"));
    const char *obs      = cJSON_GetStringValue(cJSON_GetObjectItem(j,"obs"));
    const char *sp       = cJSON_GetStringValue(cJSON_GetObjectItem(j,"status_pagamento"));
    char valor_s[32]="0", tf_s[16]="90", to_s[16]="10";
    cJSON *val = cJSON_GetObjectItem(j,"valor");
    if (!val) val = cJSON_GetObjectItem(j,"valor_total");
    if (val) snprintf(valor_s,sizeof(valor_s),"%.2f",val->valuedouble);
    cJSON *tf = cJSON_GetObjectItem(j,"taxa_fornecedor");
    if (tf && !cJSON_IsNull(tf)) snprintf(tf_s,sizeof(tf_s),"%.2f",tf->valuedouble);
    cJSON *to = cJSON_GetObjectItem(j,"taxa_operador");
    if (to && !cJSON_IsNull(to)) snprintf(to_s,sizeof(to_s),"%.2f",to->valuedouble);
    cJSON_Delete(j);
    const char *p[]={sid,valor_s,destino,data,status_v?status_v:"Pendente",obs,sp?sp:"Pendente",tf_s,to_s};
    PGresult *r = PQexecParams(conn,
        "UPDATE pedidos SET valor=$2::numeric,destino=$3,data_entrega=$4,status=$5,obs=$6,"
        "status_pagamento=$7,taxa_fornecedor=$8::numeric,taxa_operador=$9::numeric,"
        "atualizado_em=NOW() WHERE id=$1"
        " RETURNING id",
        9,NULL,p,NULL,NULL,0);
    if (PQresultStatus(r) != PGRES_TUPLES_OK) { PQclear(r); return db_err(conn, r); }
    int updated_id = atoi(PQgetvalue(r,0,0));
    PQclear(r);
    return pg_get_pedido_by_id(db, updated_id);
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
    PQexec(conn,"BEGIN");
    const char *sp[]={sid};
    PGresult *sel = PQexecParams(conn,"SELECT status FROM pedidos WHERE id=$1",1,NULL,sp,NULL,NULL,0);
    if (PQresultStatus(sel)==PGRES_TUPLES_OK && PQntuples(sel)>0) {
        const char *st = PQgetvalue(sel,0,0);
        if (strcmp(st,"Cancelado")!=0) {
            PGresult *items = PQexecParams(conn,
                "SELECT produto_id,qtd FROM itens_pedido WHERE pedido_id=$1",1,NULL,sp,NULL,NULL,0);
            if (PQresultStatus(items)==PGRES_TUPLES_OK) {
                for (int i=0; i<PQntuples(items); i++) {
                    const char *ep[]={PQgetvalue(items,i,1),PQgetvalue(items,i,0)};
                    PGresult *eu = PQexecParams(conn,"UPDATE produtos SET estoque=estoque+$1::int WHERE id=$2",2,NULL,ep,NULL,NULL,0);
                    PQclear(eu);
                }
            }
            PQclear(items);
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
        "SELECT p.id,c.nome AS cliente_nome,p.valor,p.status,p.criado_em "
        "FROM pedidos p "
        "LEFT JOIN clientes c ON c.id=p.cliente_id "
        "ORDER BY p.criado_em DESC LIMIT 5");
    if (PQresultStatus(r) != PGRES_TUPLES_OK) return db_err(conn, r);
    return ok_pedidos_com_itens(conn, r);
}

static DbResult pg_get_meus_pedidos(dp_db_t db, int usuario_id) {
    PG_CONN
    char uid_s[16]; snprintf(uid_s,sizeof(uid_s),"%d",usuario_id);
    const char *p[]={uid_s};
    PGresult *r = PQexecParams(conn,
        "SELECT p.id,p.valor,p.destino,p.data_entrega,p.status,p.criado_em,p.atualizado_em "
        "FROM pedidos p "
        "INNER JOIN clientes cl ON cl.id=p.cliente_id "
        "WHERE cl.usuario_id=$1 "
        "ORDER BY p.criado_em DESC",
        1,NULL,p,NULL,NULL,0);
    if (PQresultStatus(r) != PGRES_TUPLES_OK) return db_err(conn, r);
    return ok_pedidos_com_itens(conn, r);
}

static DbResult pg_get_meu_pedido_by_id(dp_db_t db, int usuario_id, int pedido_id) {
    PG_CONN
    char uid_s[16], pid_s[16];
    snprintf(uid_s,sizeof(uid_s),"%d",usuario_id);
    snprintf(pid_s,sizeof(pid_s),"%d",pedido_id);
    const char *p[]={uid_s,pid_s};
    PGresult *r = PQexecParams(conn,
        "SELECT p.id,p.valor,p.destino,p.data_entrega,p.status,p.obs,p.criado_em,p.atualizado_em "
        "FROM pedidos p "
        "INNER JOIN clientes cl ON cl.id=p.cliente_id "
        "WHERE cl.usuario_id=$1 AND p.id=$2",
        2,NULL,p,NULL,NULL,0);
    if (PQresultStatus(r) != PGRES_TUPLES_OK) return db_err(conn, r);
    return ok_pedido_com_itens(conn, r);
}

static DbResult pg_get_meu_pedido_status(dp_db_t db, int usuario_id, int pedido_id) {
    PG_CONN
    char uid_s[16], pid_s[16];
    snprintf(uid_s,sizeof(uid_s),"%d",usuario_id);
    snprintf(pid_s,sizeof(pid_s),"%d",pedido_id);
    const char *p[]={uid_s,pid_s};
    PGresult *r = PQexecParams(conn,
        "SELECT p.id,p.status,p.atualizado_em "
        "FROM pedidos p "
        "INNER JOIN clientes cl ON cl.id=p.cliente_id "
        "WHERE cl.usuario_id=$1 AND p.id=$2",
        2,NULL,p,NULL,NULL,0);
    if (PQresultStatus(r) != PGRES_TUPLES_OK) return db_err(conn, r);
    return ok_item(r);
}

/* â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
   DASHBOARD
   â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â• */

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
        "(SELECT COALESCE(SUM(valor * COALESCE(taxa_operador, 10) / 100),0) FROM pedidos WHERE status='Entregue') AS comissao_plataforma,"
        "(SELECT COALESCE(SUM(valor * (100 - COALESCE(taxa_operador, 10)) / 100),0) FROM pedidos WHERE status='Entregue') AS lucro_liquido,"
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

/* â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
   ESTOQUE
   â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â• */

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

/* â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
   AUTH / USUARIOS
   â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â• */

static DbResult pg_get_usuario_by_email(dp_db_t db, const char *email) {
    PG_CONN
    const char *p[]={email};
    PGresult *r = PQexecParams(conn,
        "SELECT u.id,u.nome,u.email,u.senha_hash,u.role,u.criado_em,"
        "(CASE WHEN u.role='admin' THEN u.id ELSE NULL END) AS fornecedor_id "
        "FROM usuarios u "
        "WHERE u.email=$1",
        1,NULL,p,NULL,NULL,0);
    if (PQresultStatus(r) != PGRES_TUPLES_OK) return db_err(conn, r);
    return ok_item(r);
}

static DbResult pg_get_usuario_by_id(dp_db_t db, int id) {
    PG_CONN
    char sid[16]; snprintf(sid,sizeof(sid),"%d",id);
    const char *p[]={sid};
    PGresult *r = PQexecParams(conn,
        "SELECT u.id,u.nome,u.email,u.role,u.criado_em,"
        "(CASE WHEN u.role='admin' THEN u.id ELSE NULL END) AS fornecedor_id "
        "FROM usuarios u "
        "WHERE u.id=$1",
        1,NULL,p,NULL,NULL,0);
    if (PQresultStatus(r) != PGRES_TUPLES_OK) return db_err(conn, r);
    return ok_item(r);
}

static DbResult pg_save_usuario(dp_db_t db, const char *body) {
    PG_CONN
    cJSON *j = cJSON_Parse(body);
    if (!j) { DbResult r={NULL,strdup("JSON invalido.")}; return r; }
    const char *nome  = cJSON_GetStringValue(cJSON_GetObjectItem(j, "nome"));
    const char *email = cJSON_GetStringValue(cJSON_GetObjectItem(j, "email"));
    const char *senha = cJSON_GetStringValue(cJSON_GetObjectItem(j, "senha"));
    if (!nome || !email || !senha) {
        cJSON_Delete(j);
        DbResult r={NULL,strdup("nome, email e senha sao obrigatorios.")};
        return r;
    }
    const char *role = cJSON_GetStringValue(cJSON_GetObjectItem(j, "role"));
    if (!role) role = "operador";
    const char *p[] = {nome, email, senha, role};
    PGresult *ru = PQexecParams(conn,
        "INSERT INTO usuarios(nome, email, senha_hash, role)"
        " VALUES($1, $2, $3, $4)"
        " RETURNING id, nome, email, role, criado_em, atualizado_em",
        4, NULL, p, NULL, NULL, 0);
    if (PQresultStatus(ru) != PGRES_TUPLES_OK) { cJSON_Delete(j); return db_err(conn, ru); }

    /* Captura dados do usuÃ¡rio criado */
    int user_id = atoi(PQgetvalue(ru, 0, 0));
    char uid_s[16]; snprintf(uid_s, sizeof(uid_s), "%d", user_id);
    char nome_s[256]="", email_s[256]="", role_s[32]="operador";
    snprintf(nome_s,  sizeof(nome_s),  "%s", PQgetvalue(ru, 0, 1));
    snprintf(email_s, sizeof(email_s), "%s", PQgetvalue(ru, 0, 2));
    snprintf(role_s,  sizeof(role_s),  "%s", PQgetvalue(ru, 0, 3));

    int fornecedor_id = (strcmp(role_s, "admin") == 0) ? user_id : -1;


    /* Auto-cria registro de cliente se role='cliente' */
    if (strcmp(role_s, "cliente") == 0) {
        const char *pc[] = {uid_s, nome_s, email_s};
        PGresult *rc = PQexecParams(conn,
            "INSERT INTO clientes(usuario_id, nome, email, tipo, status)"
            " VALUES($1::int, $2, $3, 'PF', 'Ativo')"
            " ON CONFLICT DO NOTHING",
            3, NULL, pc, NULL, NULL, 0);
        PQclear(rc);
    }

    /* Monta resposta JSON com fornecedor_id */
    cJSON *root = cJSON_CreateObject();
    cJSON_AddBoolToObject(root, "success", 1);
    cJSON *data = cJSON_CreateObject();
    cJSON_AddNumberToObject(data, "id", user_id);
    cJSON_AddStringToObject(data, "nome", nome_s);
    cJSON_AddStringToObject(data, "email", email_s);
    cJSON_AddStringToObject(data, "role", role_s);
    if (PQntuples(ru) > 0) {
        cJSON_AddStringToObject(data, "criado_em", PQgetvalue(ru, 0, 4));
    }
    if (fornecedor_id > 0)
        cJSON_AddNumberToObject(data, "fornecedor_id", fornecedor_id);
    else
        cJSON_AddNullToObject(data, "fornecedor_id");
    cJSON_AddItemToObject(root, "data", data);
    PQclear(ru);
    cJSON_Delete(j);
    DbResult res = { cJSON_PrintUnformatted(root), NULL };
    cJSON_Delete(root);
    return res;
}

static DbResult pg_save_usuario_with_role(dp_db_t db, const char *body, const char *role) {
    PG_CONN
    cJSON *j = cJSON_Parse(body);
    if (!j) { DbResult r={NULL,strdup("JSON invalido.")}; return r; }
    const char *nome  = cJSON_GetStringValue(cJSON_GetObjectItem(j, "nome"));
    const char *email = cJSON_GetStringValue(cJSON_GetObjectItem(j, "email"));
    const char *senha = cJSON_GetStringValue(cJSON_GetObjectItem(j, "senha"));
    if (!nome || !email || !senha) {
        cJSON_Delete(j);
        DbResult r={NULL,strdup("nome, email e senha sao obrigatorios.")};
        return r;
    }
    const char *p[] = {nome, email, senha, role};
    PGresult *r = PQexecParams(conn,
        "INSERT INTO usuarios(nome, email, senha_hash, role)"
        " VALUES($1, $2, $3, $4)"
        " RETURNING id, nome, email, role, criado_em, atualizado_em",
        4, NULL, p, NULL, NULL, 0);
    cJSON_Delete(j);
    if (PQresultStatus(r) != PGRES_TUPLES_OK) return db_err(conn, r);
    return ok_item(r);
}

static DbResult pg_get_cliente_by_usuario_id(dp_db_t db, int usuario_id) {
    PG_CONN
    char uid_s[16]; snprintf(uid_s,sizeof(uid_s),"%d",usuario_id);
    const char *p[]={uid_s};
    PGresult *r = PQexecParams(conn,
        "SELECT id,nome,tipo,doc,email,tel,cidade,estado,limite,status,criado_em,atualizado_em"
        " FROM clientes WHERE usuario_id=$1",1,NULL,p,NULL,NULL,0);
    if (PQresultStatus(r) != PGRES_TUPLES_OK) return db_err(conn, r);
    return ok_item(r);
}

static DbResult pg_get_fornecedor_by_usuario_id(dp_db_t db, int usuario_id) {
    PG_CONN
    char uid_s[16]; snprintf(uid_s,sizeof(uid_s),"%d",usuario_id);
    const char *p[]={uid_s};
    PGresult *r = PQexecParams(conn,
        "SELECT id,nome,'' AS cnpj,'' AS contato,email,'' AS tel,'' AS categoria,0 AS prazo,'Ativo' AS status,criado_em,atualizado_em"
        " FROM usuarios WHERE id=$1 AND role='admin'",1,NULL,p,NULL,NULL,0);
    if (PQresultStatus(r) != PGRES_TUPLES_OK) return db_err(conn, r);
    return ok_item(r);
}

static DbResult pg_save_cliente_for_user(dp_db_t db, int usuario_id, const char *body) {
    PG_CONN
    cJSON *j = cJSON_Parse(body);
    if (!j) { DbResult r={NULL,strdup("JSON invalido.")}; return r; }
    const char *nome  = cJSON_GetStringValue(cJSON_GetObjectItem(j,"nome"));
    const char *email = cJSON_GetStringValue(cJSON_GetObjectItem(j,"email"));
    if (!nome||!email) { cJSON_Delete(j); DbResult r={NULL,strdup("nome e email sao obrigatorios.")}; return r; }
    char uid_s[16]; snprintf(uid_s,sizeof(uid_s),"%d",usuario_id);
    char nome_s[256]="",email_s[256]="",tipo_s[32]="PF",doc_s[64]="",tel_s[64]="";
    char cidade_s[128]="",estado_s[8]="",limite_s[32]="0",status_s[64]="Ativo";
    snprintf(nome_s,sizeof(nome_s),"%s",nome);
    snprintf(email_s,sizeof(email_s),"%s",email);
    const char *tipo   = cJSON_GetStringValue(cJSON_GetObjectItem(j,"tipo"));
    const char *doc    = cJSON_GetStringValue(cJSON_GetObjectItem(j,"doc"));
    const char *tel    = cJSON_GetStringValue(cJSON_GetObjectItem(j,"tel"));
    const char *cidade = cJSON_GetStringValue(cJSON_GetObjectItem(j,"cidade"));
    const char *estado = cJSON_GetStringValue(cJSON_GetObjectItem(j,"estado"));
    if (tipo)   snprintf(tipo_s,sizeof(tipo_s),"%s",tipo);
    if (doc)    snprintf(doc_s,sizeof(doc_s),"%s",doc);
    if (tel)    snprintf(tel_s,sizeof(tel_s),"%s",tel);
    if (cidade) snprintf(cidade_s,sizeof(cidade_s),"%s",cidade);
    if (estado) snprintf(estado_s,sizeof(estado_s),"%s",estado);
    cJSON_Delete(j);
    const char *p[]={uid_s,nome_s,tipo_s,doc_s,email_s,tel_s,cidade_s,estado_s,limite_s,status_s};
    PGresult *r = PQexecParams(conn,
        "INSERT INTO clientes(usuario_id,nome,tipo,doc,email,tel,cidade,estado,limite,status)"
        " VALUES($1::int,$2,$3,$4,$5,$6,$7,$8,$9::numeric,$10)"
        " RETURNING id,nome,tipo,doc,email,tel,cidade,estado,limite,status,criado_em,atualizado_em",
        10,NULL,p,NULL,NULL,0);
    if (PQresultStatus(r) != PGRES_TUPLES_OK) return db_err(conn, r);
    return ok_item(r);
}

static DbResult pg_save_fornecedor_for_user(dp_db_t db, int usuario_id, const char *body) {
    PG_CONN
    cJSON *j = cJSON_Parse(body);
    if (!j) { DbResult r={NULL,strdup("JSON invalido.")}; return r; }
    const char *nome  = cJSON_GetStringValue(cJSON_GetObjectItem(j,"nome"));
    const char *email = cJSON_GetStringValue(cJSON_GetObjectItem(j,"email"));
    if (!nome||!email) { cJSON_Delete(j); DbResult r={NULL,strdup("nome e email sao obrigatorios.")}; return r; }
    char uid_s[16]; snprintf(uid_s,sizeof(uid_s),"%d",usuario_id);
    char nome_s[256]="",email_s[256]="",cnpj_s[32]="",contato_s[128]="";
    char tel_s[64]="",cat_s[128]="",prazo_s[16]="0",status_s[64]="Ativo";
    snprintf(nome_s,sizeof(nome_s),"%s",nome);
    snprintf(email_s,sizeof(email_s),"%s",email);
    const char *cnpj    = cJSON_GetStringValue(cJSON_GetObjectItem(j,"cnpj"));
    const char *contato = cJSON_GetStringValue(cJSON_GetObjectItem(j,"contato"));
    const char *tel     = cJSON_GetStringValue(cJSON_GetObjectItem(j,"tel"));
    const char *categoria=cJSON_GetStringValue(cJSON_GetObjectItem(j,"categoria"));
    if (cnpj)    snprintf(cnpj_s,sizeof(cnpj_s),"%s",cnpj);
    if (contato) snprintf(contato_s,sizeof(contato_s),"%s",contato);
    if (tel)     snprintf(tel_s,sizeof(tel_s),"%s",tel);
    if (categoria)snprintf(cat_s,sizeof(cat_s),"%s",categoria);
    cJSON_Delete(j);
    const char *p[]={uid_s,nome_s,cnpj_s,contato_s,email_s,tel_s,cat_s,prazo_s,status_s};
    PGresult *r = PQexecParams(conn,
        "INSERT INTO fornecedores(usuario_id,nome,cnpj,contato,email,tel,categoria,prazo,status)"
        " VALUES($1::int,$2,$3,$4,$5,$6,$7,$8::int,$9)"
        " RETURNING id,nome,cnpj,contato,email,tel,categoria,prazo,status,criado_em,atualizado_em",
        9,NULL,p,NULL,NULL,0);
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

/* â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
   CONFIG EMPRESA
   â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â• */

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
    if (!j) { DbResult r={NULL,strdup("JSON invalido.")}; return r; }
    const char *razao   = cJSON_GetStringValue(cJSON_GetObjectItem(j,"razaoSocial")); if(!razao) razao=cJSON_GetStringValue(cJSON_GetObjectItem(j,"razao_social"));
    const char *cnpj    = cJSON_GetStringValue(cJSON_GetObjectItem(j,"cnpj"));
    const char *email   = cJSON_GetStringValue(cJSON_GetObjectItem(j,"email"));
    const char *tel     = cJSON_GetStringValue(cJSON_GetObjectItem(j,"tel"));
    const char *endereco= cJSON_GetStringValue(cJSON_GetObjectItem(j,"endereco"));
    const char *p[]={razao,cnpj,email,tel,endereco};
    PGresult *r = PQexecParams(conn,
        "INSERT INTO config_empresa(razao_social,cnpj,email,tel,endereco) VALUES($1,$2,$3,$4,$5)"
        " ON CONFLICT (id) DO UPDATE SET razao_social=$1,cnpj=$2,email=$3,tel=$4,endereco=$5,atualizado_em=NOW()"
        " RETURNING id,razao_social,cnpj,email,tel,endereco,atualizado_em",
        5,NULL,p,NULL,NULL,0);

    cJSON_Delete(j);

    if (PQresultStatus(r) != PGRES_TUPLES_OK) return db_err(conn, r);
    return ok_item(r);
}

/* â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
   RELATORIOS
   â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â• */

static DbResult pg_relatorio_vendas(dp_db_t db, const char *inicio, const char *fim, int fornecedor_id) {
    PG_CONN
    char fid_s[16]; snprintf(fid_s, sizeof(fid_s), "%d", fornecedor_id);
    const char *p[]={inicio, fim, fid_s};
    const char *fid_filter = fornecedor_id > 0
        ? " AND pd.fornecedor_id=$3::int" : "";
    char sql1[512], sql2[512];
    snprintf(sql1, sizeof(sql1),
        "SELECT "
        "(SELECT COUNT(*) FROM pedidos pd WHERE pd.criado_em::date BETWEEN $1::date AND $2::date%s) AS total_pedidos,"
        "(SELECT COALESCE(SUM(ip.qtd),0) FROM itens_pedido ip INNER JOIN pedidos pd ON pd.id=ip.pedido_id WHERE pd.criado_em::date BETWEEN $1::date AND $2::date%s) AS total_itens,"
        "(SELECT COALESCE(SUM(pd.valor),0) FROM pedidos pd WHERE pd.criado_em::date BETWEEN $1::date AND $2::date%s) AS valor_total",
        fid_filter, fid_filter, fid_filter);
    snprintf(sql2, sizeof(sql2),
        "SELECT status, COUNT(*) AS total, COALESCE(SUM(valor),0) AS valor "
        "FROM pedidos pd WHERE pd.criado_em::date BETWEEN $1::date AND $2::date%s "
        "GROUP BY status ORDER BY total DESC", fid_filter);
    int nparams = fornecedor_id > 0 ? 3 : 2;
    PGresult *r = PQexecParams(conn, sql1, nparams, NULL, p, NULL, NULL, 0);
    if (PQresultStatus(r) != PGRES_TUPLES_OK) return db_err(conn, r);

    cJSON *stats = pg_to_array(r);
    cJSON *item = cJSON_DetachItemFromArray(stats, 0);
    cJSON_Delete(stats); PQclear(r);

    PGresult *r2 = PQexecParams(conn, sql2, nparams, NULL, p, NULL, NULL, 0);
    cJSON *status_arr = (PQresultStatus(r2)==PGRES_TUPLES_OK) ? pg_to_array(r2) : cJSON_CreateArray();
    PQclear(r2);

    cJSON *periodo = cJSON_CreateObject();
    cJSON_AddStringToObject(periodo, "inicio", inicio);
    cJSON_AddStringToObject(periodo, "fim", fim);
    cJSON_AddItemToObject(item, "periodo", periodo);
    cJSON_AddItemToObject(item, "pedidos_por_status", status_arr);

    cJSON *root = cJSON_CreateObject();
    cJSON_AddBoolToObject(root, "success", 1);
    cJSON_AddItemToObject(root, "data", item);
    DbResult res = { cJSON_PrintUnformatted(root), NULL };
    cJSON_Delete(root);
    return res;
}

static DbResult pg_relatorio_vendas_por_produto(dp_db_t db, const char *inicio, const char *fim, int limite, int fornecedor_id) {
    PG_CONN
    char lim_s[16]; snprintf(lim_s,sizeof(lim_s),"%d",limite>0?limite:10);
    char fid_s[16]; snprintf(fid_s,sizeof(fid_s),"%d",fornecedor_id);
    const char *p[]={inicio, fim, lim_s, fid_s};
    int nparams = fornecedor_id > 0 ? 4 : 3;
    char sql[512];
    snprintf(sql, sizeof(sql),
        "SELECT ip.produto_id, pr.nome AS produto_nome, SUM(ip.qtd) AS qtd_vendida, SUM(ip.subtotal) AS valor_total "
        "FROM itens_pedido ip "
        "INNER JOIN pedidos pd ON pd.id=ip.pedido_id "
        "INNER JOIN produtos pr ON pr.id=ip.produto_id "
        "WHERE pd.criado_em::date BETWEEN $1::date AND $2::date%s "
        "GROUP BY ip.produto_id, pr.nome ORDER BY qtd_vendida DESC LIMIT $3::int",
        fornecedor_id > 0 ? " AND pr.fornecedor_id=$4::int" : "");
    PGresult *r = PQexecParams(conn, sql, nparams, NULL, p, NULL, NULL, 0);
    if (PQresultStatus(r) != PGRES_TUPLES_OK) return db_err(conn, r);
    return ok_list(r);
}

static DbResult pg_relatorio_vendas_por_cliente(dp_db_t db, const char *inicio, const char *fim, int limite, int fornecedor_id) {
    PG_CONN
    char lim_s[16]; snprintf(lim_s,sizeof(lim_s),"%d",limite>0?limite:10);
    char fid_s[16]; snprintf(fid_s,sizeof(fid_s),"%d",fornecedor_id);
    const char *p[]={inicio, fim, lim_s, fid_s};
    int nparams = fornecedor_id > 0 ? 4 : 3;
    char sql[512];
    snprintf(sql, sizeof(sql),
        "SELECT p.cliente_id, c.nome AS cliente_nome, COUNT(p.id) AS total_pedidos, SUM(p.valor) AS valor_total "
        "FROM pedidos p "
        "INNER JOIN clientes c ON c.id=p.cliente_id "
        "WHERE p.criado_em::date BETWEEN $1::date AND $2::date%s "
        "GROUP BY p.cliente_id, c.nome ORDER BY valor_total DESC LIMIT $3::int",
        fornecedor_id > 0 ? " AND p.fornecedor_id=$4::int" : "");
    PGresult *r = PQexecParams(conn, sql, nparams, NULL, p, NULL, NULL, 0);
    if (PQresultStatus(r) != PGRES_TUPLES_OK) return db_err(conn, r);
    return ok_list(r);
}

static DbResult pg_relatorio_estoque(dp_db_t db, int fornecedor_id) {
    PG_CONN
    char fid_s[16]; snprintf(fid_s,sizeof(fid_s),"%d",fornecedor_id);
    const char *fid_filter = fornecedor_id > 0 ? " AND fornecedor_id=$1::int" : "";
    const char *p[]={fid_s};
    int nparams = fornecedor_id > 0 ? 1 : 0;
    char sql1[512], sql2[512];
    snprintf(sql1, sizeof(sql1),
        "SELECT COUNT(*) AS total_produtos, COALESCE(SUM(estoque),0) AS total_itens_estoque,"
        "COALESCE(SUM(estoque*preco),0) AS valor_total_estoque,"
        "(SELECT COUNT(*) FROM produtos WHERE estoque<=estoque_min AND status='Ativo'%s) AS produtos_abaixo_minimo "
        "FROM produtos WHERE status='Ativo'%s",
        fid_filter, fid_filter);
    snprintf(sql2, sizeof(sql2),
        "SELECT id,nome,categoria,estoque,estoque_min,preco,estoque*preco AS valor_estoque,status "
        "FROM produtos WHERE status='Ativo'%s ORDER BY nome", fid_filter);
    PGresult *r1 = PQexecParams(conn, sql1, nparams, NULL, p, NULL, NULL, 0);
    if (PQresultStatus(r1) != PGRES_TUPLES_OK) return db_err(conn, r1);

    cJSON *stats = pg_to_array(r1);
    cJSON *summary = cJSON_DetachItemFromArray(stats, 0);
    cJSON_Delete(stats); PQclear(r1);

    PGresult *r2 = PQexecParams(conn, sql2, nparams, NULL, p, NULL, NULL, 0);
    if (PQresultStatus(r2) != PGRES_TUPLES_OK) { cJSON_Delete(summary); return db_err(conn, r2); }
    cJSON *lista = pg_to_array(r2); PQclear(r2);

    cJSON_AddItemToObject(summary, "produtos", lista);
    cJSON *root = cJSON_CreateObject();
    cJSON_AddBoolToObject(root, "success", 1);
    cJSON_AddItemToObject(root, "data", summary);
    DbResult res = { cJSON_PrintUnformatted(root), NULL };
    cJSON_Delete(root);
    return res;
}

static DbResult pg_relatorio_financeiro(dp_db_t db, const char *inicio, const char *fim, int fornecedor_id) {
    PG_CONN
    char fid_s[16]; snprintf(fid_s,sizeof(fid_s),"%d",fornecedor_id);
    const char *fid_filter = fornecedor_id > 0 ? " AND fornecedor_id=$3::int" : "";
    const char *p[]={inicio, fim, fid_s};
    int nparams = fornecedor_id > 0 ? 3 : 2;
    char sql1[512];
    snprintf(sql1, sizeof(sql1),
        "SELECT COALESCE(SUM(valor),0) AS faturamento_total,"
        "COALESCE(SUM(CASE WHEN status='Entregue' THEN valor ELSE 0 END),0) AS faturamento_realizado,"
        "COALESCE(SUM(CASE WHEN status NOT IN ('Cancelado','Entregue') THEN valor ELSE 0 END),0) AS faturamento_pendente,"
        "COALESCE(SUM(valor)/NULLIF(COUNT(*),0),0) AS ticket_medio,"
        "COUNT(*) AS total_pedidos,"
        "COUNT(CASE WHEN status='Cancelado' THEN 1 END) AS pedidos_cancelados "
        "FROM pedidos WHERE criado_em::date BETWEEN $1::date AND $2::date%s", fid_filter);
    PGresult *r1 = PQexecParams(conn, sql1, nparams, NULL, p, NULL, NULL, 0);
    if (PQresultStatus(r1) != PGRES_TUPLES_OK) return db_err(conn, r1);

    cJSON *stats = pg_to_array(r1);
    cJSON *summary = cJSON_DetachItemFromArray(stats, 0);
    cJSON_Delete(stats); PQclear(r1);

    /* Detalhamento diário */
    char sql2[512];
    snprintf(sql2, sizeof(sql2),
        "SELECT TO_CHAR(criado_em, 'YYYY-MM-DD') AS dia,"
        "COUNT(*) AS total_pedidos, COALESCE(SUM(valor),0) AS valor "
        "FROM pedidos WHERE criado_em::date BETWEEN $1::date AND $2::date%s "
        "GROUP BY dia ORDER BY dia ASC", fid_filter);
    PGresult *r2 = PQexecParams(conn, sql2, nparams, NULL, p, NULL, NULL, 0);
    cJSON *diario = (PQresultStatus(r2)==PGRES_TUPLES_OK) ? pg_to_array(r2) : cJSON_CreateArray();
    PQclear(r2);

    cJSON_AddItemToObject(summary, "faturamento_por_dia", diario);
    cJSON *periodo = cJSON_CreateObject();
    cJSON_AddStringToObject(periodo, "inicio", inicio);
    cJSON_AddStringToObject(periodo, "fim", fim);
    cJSON_AddItemToObject(summary, "periodo", periodo);

    cJSON *root = cJSON_CreateObject();
    cJSON_AddBoolToObject(root, "success", 1);
    cJSON_AddItemToObject(root, "data", summary);
    DbResult res = { cJSON_PrintUnformatted(root), NULL };
    cJSON_Delete(root);
    return res;
}

/* â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
   REPOSITÃ“RIO â€” instÃ¢ncia PostgreSQL
   â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â• */

static Repository s_pg_repo = {
    .get_produtos                 = pg_get_produtos,
    .get_produto_by_id            = pg_get_produto_by_id,
    .save_produto                 = pg_save_produto,
    .update_produto               = pg_update_produto,
    .update_produto_imagem        = pg_update_produto_imagem,
    .delete_produto               = pg_delete_produto,
    .get_estoque_baixo            = pg_get_estoque_baixo,
    .get_clientes                 = pg_get_clientes,
    .get_cliente_by_id            = pg_get_cliente_by_id,
    .save_cliente                 = pg_save_cliente,
    .update_cliente               = pg_update_cliente,
    .delete_cliente               = pg_delete_cliente,
    .get_fornecedores             = pg_get_fornecedores,
    .get_fornecedor_by_id         = pg_get_fornecedor_by_id,
    .save_fornecedor              = pg_save_fornecedor,
    .update_fornecedor            = pg_update_fornecedor,
    .delete_fornecedor            = pg_delete_fornecedor,
    .get_pedidos                  = pg_get_pedidos,
    .get_pedido_by_id             = pg_get_pedido_by_id,
    .save_pedido                  = pg_save_pedido,
    .update_pedido                = pg_update_pedido,
    .update_pedido_status         = pg_update_pedido_status,
    .delete_pedido                = pg_delete_pedido,
    .get_pedidos_recentes         = pg_get_pedidos_recentes,
    .get_meus_pedidos             = pg_get_meus_pedidos,
    .get_meu_pedido_by_id         = pg_get_meu_pedido_by_id,
    .get_meu_pedido_status        = pg_get_meu_pedido_status,
    .get_dashboard_kpis           = pg_get_dashboard_kpis,
    .get_dashboard_entregas       = pg_get_dashboard_entregas,
    .get_dashboard_status_pedidos = pg_get_dashboard_status_pedidos,
    .get_estoque                  = pg_get_estoque,
    .ajustar_estoque              = pg_ajustar_estoque,
    .get_usuario_by_email         = pg_get_usuario_by_email,
    .get_usuario_by_id            = pg_get_usuario_by_id,
    .save_usuario                 = pg_save_usuario,
    .save_usuario_with_role       = pg_save_usuario_with_role,
    .get_cliente_by_usuario_id    = pg_get_cliente_by_usuario_id,
    .get_fornecedor_by_usuario_id = pg_get_fornecedor_by_usuario_id,
    .save_cliente_for_user        = pg_save_cliente_for_user,
    .save_fornecedor_for_user     = pg_save_fornecedor_for_user,
    .update_usuario_perfil        = pg_update_usuario_perfil,
    .update_usuario_senha         = pg_update_usuario_senha,
    .get_config                   = pg_get_config,
    .update_config                = pg_update_config,
    .relatorio_vendas             = pg_relatorio_vendas,
    .relatorio_vendas_por_produto = pg_relatorio_vendas_por_produto,
    .relatorio_vendas_por_cliente = pg_relatorio_vendas_por_cliente,
    .relatorio_estoque            = pg_relatorio_estoque,
    .relatorio_financeiro         = pg_relatorio_financeiro,
};

Repository* get_repository(const char *conn_str) {
    (void)conn_str;
    return &s_pg_repo;
}

Repository* repo_get_instance(void) {
    return &s_pg_repo;
}

