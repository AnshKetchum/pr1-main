#include "gfserver-student.h"
#include "gfserver.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <errno.h>
#define MAX_REQUEST_SIZE 4096
// Modify this file to implement the interface specified in
// gfserver.h.

struct gfserver_t {
    gfh_error_t (*handler)(gfcontext_t **, const char *, void*);
    void *arg;

    // This contains the server's port number
    unsigned short port;

    // Max pending requests
    int max_npending;
};

struct gfcontext_t {
    int sock;
};

gfserver_t* gfserver_create() {
    // printf("Creating socket for server ... \n");
    gfserver_t *gfs = malloc(sizeof(gfserver_t));
    memset(gfs, 0, sizeof(gfserver_t));
    gfs->port = 52507;
    gfs->max_npending = 10;
    return gfs;
}

void gfserver_set_port(gfserver_t **gfs, unsigned short port) {
    (*gfs)->port = port;
}

void gfserver_set_handlerarg(gfserver_t **gfs, void* arg) {
    (*gfs)->arg = arg;
}

void gfserver_set_handler(gfserver_t **gfs, gfh_error_t (*handler)(gfcontext_t **, const char *, void*)) {
    (*gfs)->handler = handler;
}
void gfserver_set_maxpending(gfserver_t **gfs, int max_npending) {
    (*gfs)->max_npending = max_npending;
}

void gfserver_cleanup(gfserver_t **gfs) {
    free(*gfs);
    *gfs = NULL;
}

void gfs_abort(gfcontext_t **ctx) {
    if (ctx && *ctx) {
        close((*ctx)->sock);
        free(*ctx);
        *ctx = NULL;
    }
}

ssize_t gfs_sendheader(gfcontext_t **ctx, gfstatus_t status, size_t file_len) {
    if (!ctx || !*ctx) return -1;
    char header[512];
    int len;
    const char *status_str;
    
    if (status == GF_OK) status_str = "OK";
    else if (status == GF_FILE_NOT_FOUND) status_str = "FILE_NOT_FOUND";
    else if (status == GF_ERROR) status_str = "ERROR";
    else status_str = "INVALID";
    
    if (status == GF_OK) {
        len = snprintf(header, sizeof(header), "GETFILE %s %zu\r\n\r\n", status_str, file_len);
    } else {
        len = snprintf(header, sizeof(header), "GETFILE %s\r\n\r\n", status_str);
    }
    
    return send((*ctx)->sock, header, len, 0);
}

ssize_t gfs_send(gfcontext_t **ctx, const void *data, size_t len) {
    if (!ctx || !*ctx) return -1;
    size_t total_sent = 0;
    const char *ptr = data;
    while (total_sent < len) {
        ssize_t sent = send((*ctx)->sock, ptr + total_sent, len - total_sent, 0);
        if (sent <= 0) return sent;
        total_sent += sent;
    }
    return total_sent;
}

void gfserver_serve(gfserver_t **gfs) {
    if (!gfs || !*gfs) return;
    gfserver_t *server = *gfs;
    
    int listen_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_sock == -1) return;
    
    int opt = 1;
    setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(server->port);
    
    if (bind(listen_sock, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        close(listen_sock);
        return;
    }
    
    if (listen(listen_sock, server->max_npending) == -1) {
        close(listen_sock);
        return;
    }
    
    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_sock = accept(listen_sock, (struct sockaddr *)&client_addr, &client_len);
        if (client_sock == -1) continue;
        
        char buffer[2048];
        ssize_t bytes_received = 0;
        while (bytes_received < sizeof(buffer) - 1) {
            ssize_t n = recv(client_sock, buffer + bytes_received, 1, 0);
            if (n <= 0) break;
            bytes_received++;
            buffer[bytes_received] = '\0';
            if (strstr(buffer, "\r\n\r\n")) break;
        }
        
        char scheme[16], method[16], path[1024];
        if (sscanf(buffer, "%15s %15s %1023s", scheme, method, path) == 3 &&
            strcmp(scheme, "GETFILE") == 0 && strcmp(method, "GET") == 0 && path[0] == '/') {
            
            gfcontext_t *ctx = malloc(sizeof(gfcontext_t));
            ctx->sock = client_sock;
            if (server->handler) {
                server->handler(&ctx, path, server->arg);
            }
            if (ctx) {
                close(ctx->sock);
                free(ctx);
            }
        } else {
            // Invalid request
            const char *invalid_resp = "GETFILE INVALID\r\n\r\n";
            send(client_sock, invalid_resp, strlen(invalid_resp), 0);
            close(client_sock);
        }
    }
    close(listen_sock);
}


