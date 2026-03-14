#ifndef DISTRIBPRO_H
#define DISTRIBPRO_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// Forward declarations for libraries (we'll assume they are linked)
// Mongoose for HTTP
// cJSON for JSON

// Constants
#define API_VERSION "/api/v1"
#define DEFAULT_PORT "8000"

// Error codes
typedef enum {
    SUCCESS = 0,
    VALIDATION_ERROR,
    NOT_FOUND,
    DB_ERROR,
    UNAUTHORIZED
} dp_error_t;

// Response structure
typedef struct {
    int success;
    char *data;      // JSON string
    char *error_msg;
    int status_code;
} dp_response_t;

// Database connection handle (opaque)
typedef void* dp_db_t;

// Initialize Database
dp_db_t db_init(const char *conn_str);
void db_close(dp_db_t db);

// Auth functions
char* auth_login(const char *email, const char *password);
int auth_verify_token(const char *token);

#endif // DISTRIBPRO_H
