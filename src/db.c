#include "distribpro.h"
#include "distribpro/db.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libpq-fe.h>

// If we want to support both, we'd need some conditional compilation or a dynamic dispatch
// For simplicity, we'll implement a basic abstraction

typedef struct {
    int is_postgres;
    void *conn; // PGconn* or sqlite3*
} db_handle_t;

dp_db_t db_init(const char *conn_str) {
    // Initialize repository singleton
    get_repository(conn_str);

    db_handle_t *handle = malloc(sizeof(db_handle_t));
    if (!handle) return NULL;

    // Detecta PostgreSQL tanto pelo formato URI quanto pelo formato chave-valor
    if (conn_str && (strstr(conn_str, "postgresql://") != NULL || strstr(conn_str, "host=") != NULL)) {
        handle->is_postgres = 1;
        printf("Connecting to Supabase (Postgres)...\n");
        handle->conn = PQconnectdb(conn_str);
        if (PQstatus((PGconn*)handle->conn) != CONNECTION_OK) {
            fprintf(stderr, "Connection to database failed: %s\n", PQerrorMessage((PGconn*)handle->conn));
            PQfinish((PGconn*)handle->conn); // Clean up failed connection
            free(handle);
            return NULL;
        }
    } else {
        handle->is_postgres = 0;
        printf("Connecting to Local DB (SQLite)...\n");
        // sqlite3_open(conn_str, (sqlite3**)&handle->conn);
    }

    return (dp_db_t)handle;
}

void db_close(dp_db_t db) {
    db_handle_t *handle = (db_handle_t*)db;
    if (!handle) return;
    
    if (handle->is_postgres) {
        PQfinish((PGconn*)handle->conn);
    } else {
        // sqlite3_close((sqlite3*)handle->conn);
    }
    free(handle);
}

void* db_get_pg_conn(dp_db_t db) {
    db_handle_t *handle = (db_handle_t*)db;
    if (!handle || !handle->is_postgres) {
        return NULL;
    }
    return handle->conn;
}
