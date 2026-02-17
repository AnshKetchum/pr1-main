#include "gfclient-student.h"
#include "gfclient.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <errno.h>
#define MAX_REQUEST_SIZE 4096
 // Modify this file to implement the interface specified in
 // gfclient.h.

struct gfcrequest_t {
    // Define our scheme
    char *path;
    // Define our port
    unsigned short port;

    // Define our server
    char *server;

    // Define our writefunc
    void (*writefunc)(void *, size_t, void *);
    void *writearg;

    // Define our headerfunc
    void (*headerfunc)(void *, size_t, void *);
    void *headerarg;
    // Use gf_status to tell how the response went
    gfstatus_t status;
    size_t filelen;
    size_t bytesreceived;
};

void gfc_global_init() {}
void gfc_global_cleanup() {}

gfcrequest_t *gfc_create() {
    gfcrequest_t *gfr = malloc(sizeof(gfcrequest_t));
    if (gfr) {
        memset(gfr, 0, sizeof(gfcrequest_t));
        gfr->status = GF_INVALID;
    }
    return gfr;
}

void gfc_set_port(gfcrequest_t **gfr, unsigned short port) {
    (*gfr)->port = port;
}

void gfc_set_server(gfcrequest_t **gfr, const char* server) {
    if (gfr && *gfr) {
        if ((*gfr)->server) free((*gfr)->server);
        (*gfr)->server = strdup(server);
    }
}

void gfc_set_path(gfcrequest_t **gfr, const char* path) {
    if (gfr && *gfr) {
        if ((*gfr)->path) free((*gfr)->path);
        (*gfr)->path = strdup(path);
    }
}

void gfc_set_headerfunc(gfcrequest_t **gfr, void (*headerfunc)(void *, size_t, void *)) {
    (*gfr)->headerfunc = headerfunc;
}

void gfc_set_headerarg(gfcrequest_t **gfr, void *headerarg) {
    (*gfr)->headerarg = headerarg;
}

void gfc_set_writefunc(gfcrequest_t **gfr, void (*writefunc)(void *, size_t, void *)) {
    (*gfr)->writefunc = writefunc;
}

void gfc_set_writearg(gfcrequest_t **gfr, void *writearg) {
    (*gfr)->writearg = writearg;
}

gfstatus_t gfc_get_status(gfcrequest_t **gfr) {
    return (gfr && *gfr) ? (*gfr)->status : GF_INVALID;
}

size_t gfc_get_filelen(gfcrequest_t **gfr) {
    return (gfr && *gfr) ? (*gfr)->filelen : 0;
}

size_t gfc_get_bytesreceived(gfcrequest_t **gfr) {
    return (gfr && *gfr) ? (*gfr)->bytesreceived : 0;
}

void gfc_cleanup(gfcrequest_t **gfr) {
    if (gfr && *gfr) {
        if ((*gfr)->server) free((*gfr)->server);
        if ((*gfr)->path) free((*gfr)->path);
        free(*gfr);
        *gfr = NULL;
    }
}

int gfc_perform(gfcrequest_t **gfr) {

    // bad server
    if (!gfr || !*gfr || !(*gfr)->server || !(*gfr)->path) {
      return -1;
    }
    gfcrequest_t *req = *gfr;
    
    char portstr[16];
    snprintf(portstr, sizeof(portstr), "%u", req->port);
    
    struct addrinfo hints, *res;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    
    if (getaddrinfo(req->server, portstr, &hints, &res) != 0) {
      return -1;
    }
    
    int sock = -1;
    struct addrinfo *p;
    for (p = res; p != NULL; p = p->ai_next) {
        sock = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (sock == -1) continue;
        if (connect(sock, p->ai_addr, p->ai_addrlen) == 0) break;
        close(sock);
        sock = -1;
    }
    freeaddrinfo(res);
    if (sock == -1) return -1;
    

    // Issue the request
    char request[MAX_REQUEST_SIZE];
    int reqlen = snprintf(request, sizeof(request), "GETFILE GET %s\r\n\r\n", req->path);
    if (send(sock, request, reqlen, 0) != reqlen) {
        close(sock);
        return -1;
    }
    
    char buffer[MAX_REQUEST_SIZE];
    ssize_t bytes_read;
    size_t total_header_len = 0;
    char *header_end = NULL;
    
    // Recv header
    while (total_header_len < sizeof(buffer) - 1) {
        bytes_read = recv(sock, buffer + total_header_len, 1, 0);
        if (bytes_read <= 0) {
            close(sock);
            return -1;
        }
        total_header_len++;
        buffer[total_header_len] = '\0';
        header_end = strstr(buffer, "\r\n\r\n");
        if (header_end) break;
    }
    
    if (!header_end) {
        close(sock);
        return -1;
    }
    
    size_t header_len = (header_end + 4) - buffer;
    if (req->headerfunc) req->headerfunc(buffer, header_len, req->headerarg);
    
    char scheme[16], status_str[32];
    unsigned long long length = 0;
    int items = sscanf(buffer, "%15s %31s %llu", scheme, status_str, &length);
    
    if (items < 2 || strcmp(scheme, "GETFILE") != 0) {
        req->status = GF_INVALID;
        close(sock);
        return -1;
    }
    
    if (strcmp(status_str, "OK") == 0) {
        req->status = GF_OK;
        req->filelen = (size_t)length;
    } else if (strcmp(status_str, "FILE_NOT_FOUND") == 0) {
        req->status = GF_FILE_NOT_FOUND;
    } else if (strcmp(status_str, "ERROR") == 0) {
        req->status = GF_ERROR;
    } else {
        req->status = GF_INVALID;
    }
    
    // receive the actual bytes
    req->bytesreceived = 0;
    if (req->status == GF_OK) {
        while ((bytes_read = recv(sock, buffer, sizeof(buffer), 0)) > 0) {
            if (req->writefunc) req->writefunc(buffer, bytes_read, req->writearg);
            req->bytesreceived += bytes_read;
        }
    }
    
    close(sock);
    return 0;
}

const char *gfc_strstatus(gfstatus_t status) {
    switch(status) {
        case GF_OK: return "OK";
        case GF_FILE_NOT_FOUND: return "FILE_NOT_FOUND";
        case GF_ERROR: return "ERROR";
        case GF_INVALID: return "INVALID";
        default: return "UNKNOWN";
    }
}