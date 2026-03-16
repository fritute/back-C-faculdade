/*
 * supabase_storage.c — Utilitário para upload de arquivos ao Supabase Storage
 *
 * Variáveis de ambiente necessárias:
 *   SUPABASE_URL           ex.: https://xyzabc.supabase.co
 *   SUPABASE_SERVICE_KEY   Service Role Key (JWT) do projeto
 *   SUPABASE_BUCKET        Nome do bucket (padrão: "produtos")
 */

#include "distribpro/supabase_storage.h"
#include <curl/curl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ── Buffer para a resposta do curl ──────────────────────── */

typedef struct {
    char  *buf;
    size_t len;
} CurlBuf;

static size_t curl_write_cb(void *data, size_t sz, size_t nmemb, void *userp) {
    size_t total = sz * nmemb;
    CurlBuf *b = (CurlBuf *)userp;
    char *tmp = realloc(b->buf, b->len + total + 1);
    if (!tmp) return 0;
    b->buf = tmp;
    memcpy(b->buf + b->len, data, total);
    b->len += total;
    b->buf[b->len] = '\0';
    return total;
}

/* ── Gera um nome de arquivo único no bucket ─────────────── */

static void make_object_path(char *out, size_t out_len,
                              int produto_id, const char *mime_type) {
    const char *ext = "jpg";
    if (strstr(mime_type, "png"))  ext = "png";
    else if (strstr(mime_type, "gif"))  ext = "gif";
    else if (strstr(mime_type, "webp")) ext = "webp";

    snprintf(out, out_len, "produto_%d_%ld.%s",
             produto_id, (long)time(NULL), ext);
}

/* ── Função principal de upload ──────────────────────────── */

/*
 * Faz upload de `data` (bytes) para o Supabase Storage.
 * Retorna a URL pública em `out_url` (buffer de pelo menos 512 bytes).
 * Retorna 0 em sucesso, -1 em falha.
 */
int supabase_upload_image(int produto_id,
                          const char *data, size_t data_len,
                          const char *mime_type,
                          char *out_url, size_t out_url_len)
{
    const char *base_url = getenv("SUPABASE_URL");
    const char *svc_key  = getenv("SUPABASE_SERVICE_KEY");
    const char *bucket   = getenv("SUPABASE_BUCKET");

    if (!base_url || !svc_key) {
        fprintf(stderr, "[supabase_storage] SUPABASE_URL ou SUPABASE_SERVICE_KEY nao definidos.\n");
        return -1;
    }
    if (!bucket || bucket[0] == '\0') bucket = "imagens_produtos";
    if (!mime_type || mime_type[0] == '\0') mime_type = "image/jpeg";

    /* Caminho do objeto dentro do bucket */
    char obj_path[256];
    make_object_path(obj_path, sizeof(obj_path), produto_id, mime_type);

    /* URL do endpoint da Storage API */
    char upload_url[1024];
    snprintf(upload_url, sizeof(upload_url),
             "%s/storage/v1/object/%s/%s", base_url, bucket, obj_path);

    /* Cabeçalho de autorização */
    char auth_header[512];
    snprintf(auth_header, sizeof(auth_header), "Authorization: Bearer %s", svc_key);

    /* Cabeçalho Content-Type */
    char ct_header[128];
    snprintf(ct_header, sizeof(ct_header), "Content-Type: %s", mime_type);

    CURL *curl = curl_easy_init();
    if (!curl) {
        fprintf(stderr, "[supabase_storage] curl_easy_init falhou.\n");
        return -1;
    }

    CurlBuf resp = { NULL, 0 };

    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, auth_header);
    headers = curl_slist_append(headers, ct_header);
    headers = curl_slist_append(headers, "x-upsert: true"); /* sobrescreve se existir */

    curl_easy_setopt(curl, CURLOPT_URL,            upload_url);
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST,  "POST");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER,     headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS,     data);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE,  (long)data_len);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION,  curl_write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA,      &resp);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);

    CURLcode res = curl_easy_perform(curl);

    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        fprintf(stderr, "[supabase_storage] curl error: %s\n", curl_easy_strerror(res));
        free(resp.buf);
        return -1;
    }

    if (http_code < 200 || http_code >= 300) {
        fprintf(stderr, "[supabase_storage] HTTP %ld: %s\n",
                http_code, resp.buf ? resp.buf : "(sem corpo)");
        free(resp.buf);
        return -1;
    }

    free(resp.buf);

    /* Monta URL pública */
    snprintf(out_url, out_url_len,
             "%s/storage/v1/object/public/%s/%s", base_url, bucket, obj_path);

    return 0;
}
