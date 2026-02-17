#include "gfserver-student.h"
#include "stdio.h"
#include "stdlib.h"
#include <sys/types.h>
#include <sys/socket.h>
#define MAX_REQUEST_SIZE 2048

// Modify this file to implement the interface specified in
// gfserver.h.
int sockfd = -1;  
struct sockaddr_in serv_addr, cli_addr;
char buffer[MAX_REQUEST_SIZE];

struct gfserver_t {

    gfh_error_t (*handler)(gfcontext_t **, const char *, void*);
    void* arg;

    // This contains the server's port number
    unsigned short port;

    // Max pending requests
    int max_npending;
};

void gfs_abort(gfcontext_t **ctx){
    if(sockfd > -1) {
        close(sockfd);
    }
}

ssize_t gfs_send(gfcontext_t **ctx, const void *data, size_t len){
    // not yet implemented
    return -1;
}

ssize_t gfs_sendheader(gfcontext_t **ctx, gfstatus_t status, size_t file_len){
    // not yet implemented
    return -1;
}

gfserver_t* gfserver_create(){
    printf("Creating socket for server ... \n");
    sockfd = socket(AF_INET, SOCK_STREAM, 0);

    gfserver_t* server = (gfserver_t*)(malloc(sizeof(gfserver_t)));
    return server;
}

void gfserver_set_port(gfserver_t **gfs, unsigned short port){
    (*gfs)->port = port;
}

void gfserver_bind(gfserver_t **gfs) {

    // Creates the server instance
    bzero((char *) &serv_addr, sizeof(serv_addr));
    int portno = (*gfs)->port;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(portno);

    // Bind to socket
     if (bind(sockfd, (struct sockaddr *) &serv_addr,
              sizeof(serv_addr)) < 0)  {
                printf("Error on binding - server failed to set up connection.\n");
    }

    printf("Server is bound to port %d\n", portno);
}

void gfserver_send(char* content, int length) {
    snprintf(buffer, sizeof(buffer), "%s %s %s\r\n\r\n");
}

void gfserver_serve(gfserver_t **gfs){
    if(sockfd == -1) {
        return; // we don't have a valid socket instance
    }

    // Bind socket
    gfserver_bind(gfs);

    // Accept requests
    while(1) {
        int newsockfd, n; 
        socklen_t clilen = sizeof(cli_addr);

        listen(sockfd, 5);
        clilen = sizeof(cli_addr);
        newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);

        bzero(buffer,256);
        n = read(newsockfd,buffer,255);
        if (n <= 0) {
            printf("Client disconnect\n");
        }

        printf("Here is the message: %s\n",buffer);

        close(newsockfd);
    }
}

void gfserver_set_handlerarg(gfserver_t **gfs, void* arg){
    (*gfs)->arg = arg;
}

void gfserver_set_handler(gfserver_t **gfs, gfh_error_t (*handler)(gfcontext_t **, const char *, void*)){
    (*gfs)->handler = handler;
}

void gfserver_set_maxpending(gfserver_t **gfs, int max_npending){
    (*gfs)->max_npending = max_npending;
}

void gfserver_cleanup(gfserver_t **gfs) {
    free(*gfs);
    *gfs = NULL;
}
