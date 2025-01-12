/*
  proxy_parse.c -- a HTTP Request Parsing Library.
  COS 461  
*/

#include "proxy_parse.h"

#define DEFAULT_NHDRS 8
#define MAX_REQ_LEN 65535
#define MIN_REQ_LEN 4

static const char *root_abs_path = "/";

/* private function declarations */
int ParsedRequest_printRequestLine(struct ParsedRequest *pr, char *buf, size_t buflen, size_t *tmp);
size_t ParsedRequest_requestLineLen(struct ParsedRequest *pr);

/*
 * debug() prints out debugging info if DEBUG is set to 1
 *
 * parameter format: same as printf 
 */
void debug(const char *format, ...) {
    va_list args;
    if (DEBUG) {
        va_start(args, format);
        vfprintf(stderr, format, args);
        va_end(args);
    }
}

/*
 *  ParsedHeader Public Methods
 */

/* Set a header with key and value */
int ParsedHeader_set(struct ParsedRequest *pr, const char *key, const char *value) {
    struct ParsedHeader *ph;
    ParsedHeader_remove(pr, key);

    if (pr->headerslen <= pr->headersused + 1) {
        pr->headerslen *= 2;
        pr->headers = (struct ParsedHeader *)realloc(pr->headers, pr->headerslen * sizeof(struct ParsedHeader));
        if (!pr->headers)
            return -1;
    }

    ph = pr->headers + pr->headersused;
    pr->headersused += 1;

    ph->key = strdup(key);
    ph->value = strdup(value);

    ph->keylen = strlen(key) + 1;
    ph->valuelen = strlen(value) + 1;
    return 0;
}

/* Get the parsedHeader with the specified key or NULL */
struct ParsedHeader* ParsedHeader_get(struct ParsedRequest *pr, const char *key) {
    for (size_t i = 0; i < pr->headersused; i++) {
        struct ParsedHeader *tmp = pr->headers + i;
        if (tmp->key && strcmp(tmp->key, key) == 0) {
            return tmp;
        }
    }
    return NULL;
}

/* Remove the specified key from parsedHeader */
int ParsedHeader_remove(struct ParsedRequest *pr, const char *key) {
    struct ParsedHeader *tmp = ParsedHeader_get(pr, key);
    if (!tmp)
        return -1;

    free(tmp->key);
    free(tmp->value);
    tmp->key = NULL;
    return 0;
}

/*
  ParsedHeader Private Methods
*/

void ParsedHeader_create(struct ParsedRequest *pr) {
    pr->headers = (struct ParsedHeader *)malloc(sizeof(struct ParsedHeader) * DEFAULT_NHDRS);
    pr->headerslen = DEFAULT_NHDRS;
    pr->headersused = 0;
}

size_t ParsedHeader_lineLen(struct ParsedHeader *ph) {
    return ph->key ? strlen(ph->key) + strlen(ph->value) + 4 : 0;
}

size_t ParsedHeader_headersLen(struct ParsedRequest *pr) {
    if (!pr || !pr->buf)
        return 0;

    size_t len = 0;
    for (size_t i = 0; i < pr->headersused; i++) {
        len += ParsedHeader_lineLen(pr->headers + i);
    }
    return len + 2;
}

int ParsedHeader_printHeaders(struct ParsedRequest *pr, char *buf, size_t len) {
    char *current = buf;

    if (len < ParsedHeader_headersLen(pr)) {
        debug("buffer for printing headers too small\n");
        return -1;
    }

    for (size_t i = 0; i < pr->headersused; i++) {
        struct ParsedHeader *ph = pr->headers + i;
        if (ph->key) {
            memcpy(current, ph->key, strlen(ph->key));
            memcpy(current + strlen(ph->key), ": ", 2);
            memcpy(current + strlen(ph->key) + 2, ph->value, strlen(ph->value));
            memcpy(current + strlen(ph->key) + 2 + strlen(ph->value), "\r\n", 2);
            current += strlen(ph->key) + strlen(ph->value) + 4;
        }
    }
    memcpy(current, "\r\n", 2);
    return 0;
}

void ParsedHeader_destroyOne(struct ParsedHeader *ph) {
    if (ph->key) {
        free(ph->key);
        free(ph->value);
        ph->key = NULL;
        ph->value = NULL;
        ph->keylen = 0;
        ph->valuelen = 0;
    }
}

void ParsedHeader_destroy(struct ParsedRequest *pr) {
    for (size_t i = 0; i < pr->headersused; i++) {
        ParsedHeader_destroyOne(pr->headers + i);
    }
    pr->headersused = 0;
    free(pr->headers);
    pr->headerslen = 0;
}

int ParsedHeader_parse(struct ParsedRequest *pr, char *line) {
    char *index1 = strchr(line, ':');
    if (!index1) {
        debug("No colon found\n");
        return -1;
    }

    char *key = strndup(line, index1 - line);
    index1 += 2;
    char *index2 = strstr(index1, "\r\n");
    char *value = strndup(index1, index2 - index1);

    ParsedHeader_set(pr, key, value);
    free(key);
    free(value);
    return 0;
}

/*
  ParsedRequest Public Methods
*/

void ParsedRequest_destroy(struct ParsedRequest *pr) {
    if (pr->buf)
        free(pr->buf);
    if (pr->path)
        free(pr->path);
    if (pr->headerslen > 0)
        ParsedHeader_destroy(pr);
    free(pr);
}

struct ParsedRequest* ParsedRequest_create() {
    struct ParsedRequest *pr = (struct ParsedRequest *)malloc(sizeof(struct ParsedRequest));
    if (pr) {
        ParsedHeader_create(pr);
        pr->buf = NULL;
        pr->method = NULL;
        pr->protocol = NULL;
        pr->host = NULL;
        pr->path = NULL;
        pr->version = NULL;
        pr->buflen = 0;
    }
    return pr;
}

/* 
   Recreate the entire buffer from a parsed request object.
   buf must be allocated
*/
int ParsedRequest_unparse(struct ParsedRequest *pr, char *buf, size_t buflen) {
    if (!pr || !pr->buf)
        return -1;

    size_t tmp;
    if (ParsedRequest_printRequestLine(pr, buf, buflen, &tmp) < 0)
        return -1;
    if (ParsedHeader_printHeaders(pr, buf + tmp, buflen - tmp) < 0)
        return -1;
    return 0;
}

/* 
   Recreate the headers from a parsed request object.
   buf must be allocated
*/
int ParsedRequest_unparse_headers(struct ParsedRequest *pr, char *buf, size_t buflen) {
    if (!pr || !pr->buf)
        return -1;

    if (ParsedHeader_printHeaders(pr, buf, buflen) < 0)
        return -1;
    return 0;
}

/* Size of the headers if unparsed into a string */
size_t ParsedRequest_totalLen(struct ParsedRequest *pr) {
    if (!pr || !pr->buf)
        return 0;
    return ParsedRequest_requestLineLen(pr) + ParsedHeader_headersLen(pr);
}

/* 
   Parse request buffer
   Parameters: 
   parse: ptr to a newly created ParsedRequest object
   buf: ptr to the buffer containing the request (need not be NUL terminated)
   and the trailing \r\n\r\n
   buflen: length of the buffer including the trailing \r\n\r\n
   Return values:
   -1: failure
   0: success
*/
int ParsedRequest_parse(struct ParsedRequest *parse, const char *buf, int buflen) {
    if (parse->buf) {
        debug("parse object already assigned to a request\n");
        return -1;
    }

    if (buflen < MIN_REQ_LEN || buflen > MAX_REQ_LEN) {
        debug("invalid buflen %d", buflen);
        return -1;
    }

    char *tmp_buf = strndup(buf, buflen);
    char *index = strstr(tmp_buf, "\r\n\r\n");
    if (!index) {
        debug("invalid request line, no end of header\n");
        free(tmp_buf);
        return -1;
    }

    index = strstr(tmp_buf, "\r\n");
    parse->buf = strndup(tmp_buf, index - tmp_buf);

    char *saveptr;
    parse->method = strtok_r(parse->buf, " ", &saveptr);
    if (!parse->method || strcmp(parse->method, "GET")) {
        debug("invalid request line, method not 'GET': %s\n", parse->method);
        free(tmp_buf);
        free(parse->buf);
        parse->buf = NULL;
        return -1;
    }

    char *full_addr = strtok_r(NULL, " ", &saveptr);
    if (!full_addr) {
        debug("invalid request line, no full address\n");
        free(tmp_buf);
        free(parse->buf);
        parse->buf = NULL;
        return -1;
    }

    parse->version = full_addr + strlen(full_addr) + 1;
    if (!parse->version || strncmp(parse->version, "HTTP/", 5)) {
        debug("invalid request line, unsupported version %s\n", parse->version);
        free(tmp_buf);
        free(parse->buf);
        parse->buf = NULL;
        return -1;
    }

    parse->protocol = strtok_r(full_addr, "://", &saveptr);
    if (!parse->protocol) {
        debug("invalid request line, missing host\n");
        free(tmp_buf);
        free(parse->buf);
        parse->buf = NULL;
        return -1;
    }

    const char *rem = full_addr + strlen(parse->protocol) + strlen("://");
    size_t abs_uri_len = strlen(rem);

    parse->host = strtok_r(NULL, "/", &saveptr);
    if (!parse->host || strlen(parse->host) == abs_uri_len) {
        debug("invalid request line, missing host or absolute path\n");
        free(tmp_buf);
        free(parse->buf);
        parse->buf = NULL;
        return -1;
    }

    parse->path = strtok_r(NULL, " ", &saveptr);
    if (!parse->path) {
        parse->path = strdup(root_abs_path);
    } else if (strncmp(parse->path, root_abs_path, strlen(root_abs_path)) == 0) {
        debug("invalid request line, path cannot begin with two slash characters\n");
        free(tmp_buf);
        free(parse->buf);
        parse->buf = NULL;
        parse->path = NULL;
        return -1;
    } else {
        char *tmp_path = parse->path;
        parse->path = (char *)malloc(strlen(root_abs_path) + strlen(tmp_path) + 1);
        strcpy(parse->path, root_abs_path);
        strcat(parse->path, tmp_path);
    }

    parse->host = strtok_r(parse->host, ":", &saveptr);
    parse->port = strtok_r(NULL, "/", &saveptr);

    if (!parse->host) {
        debug("invalid request line, missing host\n");
        free(tmp_buf);
        free(parse->buf);
        free(parse->path);
        parse->buf = NULL;
        parse->path = NULL;
        return -1;
    }

    if (parse->port) {
        int port = strtol(parse->port, NULL, 10);
        if (port == 0 && errno == EINVAL) {
            debug("invalid request line, bad port: %s\n", parse->port);
            free(tmp_buf);
            free(parse->buf);
            free(parse->path);
            parse->buf = NULL;
            parse->path = NULL;
            return -1;
        }
    }

    int ret = 0;
    char *currentHeader = strstr(tmp_buf, "\r\n") + 2;
    while (currentHeader[0] != '\0' && !(currentHeader[0] == '\r' && currentHeader[1] == '\n')) {
        if (ParsedHeader_parse(parse, currentHeader)) {
            ret = -1;
            break;
        }
        currentHeader = strstr(currentHeader, "\r\n");
        if (!currentHeader || strlen(currentHeader) < 2)
            break;
        currentHeader += 2;
    }
    free(tmp_buf);
    return ret;
}

/* 
   ParsedRequest Private Methods
*/

size_t ParsedRequest_requestLineLen(struct ParsedRequest *pr) {
    if (!pr || !pr->buf)
        return 0;

    size_t len = strlen(pr->method) + 1 + strlen(pr->protocol) + 3 + strlen(pr->host) + 1 + strlen(pr->version) + 2;
    if (pr->port)
        len += strlen(pr->port) + 1;
    len += strlen(pr->path);
    return len;
}

int ParsedRequest_printRequestLine(struct ParsedRequest *pr, char *buf, size_t buflen, size_t *tmp) {
    char *current = buf;

    if (buflen < ParsedRequest_requestLineLen(pr)) {
        debug("not enough memory for first line\n");
        return -1;
    }

    memcpy(current, pr->method, strlen(pr->method));
    current += strlen(pr->method);
    *current++ = ' ';

    memcpy(current, pr->protocol, strlen(pr->protocol));
    current += strlen(pr->protocol);
    memcpy(current, "://", 3);
    current += 3;
    memcpy(current, pr->host, strlen(pr->host));
    current += strlen(pr->host);
    if (pr->port) {
        *current++ = ':';
        memcpy(current, pr->port, strlen(pr->port));
        current += strlen(pr->port);
    }
    memcpy(current, pr->path, strlen(pr->path));
    current += strlen(pr->path);

    *current++ = ' ';
    memcpy(current, pr->version, strlen(pr->version));
    current += strlen(pr->version);
    memcpy(current, "\r\n", 2);
    current += 2;
    *tmp = current - buf;
    return 0;
}