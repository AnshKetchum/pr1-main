
#include <stdlib.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/socket.h>
#include "gfclient-student.h"
#define MAX_REQUEST_SIZE 2048

 // Modify this file to implement the interface specified in
 // gfclient.h.

struct gfcrequest_t {
  // Define our scheme
  char *gfc_scheme;
  char *gfc_method;
  const char *path;

  // Define our port
  unsigned short port;

  // Define our server
  const char *server;

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

int sockfd = -1; // Sockfd = -1 means we haven't set up the socket 
struct sockaddr_in serv_addr;
struct hostent *server;

// Max request size
char data_to_send[MAX_REQUEST_SIZE];

// optional function for cleaup processing.
void gfc_cleanup(gfcrequest_t **gfr) {
  printf("Destroying GFC request \n");
  free(*gfr);
  *gfr = NULL;

  close(sockfd);
}

gfcrequest_t *gfc_create() {
  printf("Create GFC request \n");

  // Initialize our socket
  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0) {
    printf("ERROR opening socket\n");
  }

  gfcrequest_t* gfc_request = (gfcrequest_t*)(malloc(sizeof(gfcrequest_t)));
  return gfc_request;
}

size_t gfc_get_bytesreceived(gfcrequest_t **gfr) {
  // not yet implemented
  size_t bytes_received = (*gfr)->bytesreceived;
  return bytes_received;
}

gfstatus_t gfc_get_status(gfcrequest_t **gfr) {
  gfstatus_t return_status = (*gfr)->status;
  return return_status;
}

size_t gfc_get_filelen(gfcrequest_t **gfr) {
  // not yet implemented
  size_t filelen = (*gfr)->filelen;
  return filelen;
}

void gfc_global_init() {
  printf("Called global init for the GFC protocol\n");

}

void gfc_global_cleanup() {
  printf("Called global cleanup for the GFC protocol\n");
}


int gfc_perform(gfcrequest_t **gfr) {
  // not yet implemented

  server = gethostbyname((*gfr)->server);
  if (server == NULL) {
    (*gfr)->status = GF_ERROR;
    return -1;
  }

  bzero((char *) &serv_addr, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  bcopy((char *)server->h_addr, 
        (char *)&serv_addr.sin_addr.s_addr,
        server->h_length);
  serv_addr.sin_port = htons((*gfr)->port);

  if (connect(sockfd,(struct sockaddr *)&serv_addr,sizeof(serv_addr)) < 0) {
    (*gfr)->status = GF_ERROR;
    printf("Connection failure.\n");
    return -1;
  }

  // Needs to be in the format of <scheme> <method> <path>\r\n\r\n
  snprintf(data_to_send, sizeof(data_to_send), "%s %s %s\r\n\r\n", "GETFILE", "GET", (*gfr)->path);
  int number_of_bytes_sent = write(sockfd, data_to_send, strlen(data_to_send));

  if(number_of_bytes_sent < 0) {
    (*gfr)->status = GF_ERROR;
    printf("Failed to send data.\n");
    return -1;
  } else { //Success case!
    (*gfr)->status = GF_OK;
    printf("Successfully sent %d bytes.\n", number_of_bytes_sent);
    return 0;
  }

  // Success!
  return 0;
}

void gfc_set_path(gfcrequest_t **gfr, const char *path) {
  (*gfr)->path = path;
}

void gfc_set_headerfunc(gfcrequest_t **gfr, void (*headerfunc)(void *, size_t, void *)) {
  (*gfr)->headerfunc = headerfunc;
}

void gfc_set_server(gfcrequest_t **gfr, const char *server) {
  (*gfr)->server = server;
}

void gfc_set_port(gfcrequest_t **gfr, unsigned short port) {
  (*gfr)->port = port;
}

void gfc_set_writearg(gfcrequest_t **gfr, void *writearg) {
  (*gfr)->writearg = writearg;
}

void gfc_set_headerarg(gfcrequest_t **gfr, void *headerarg) {
  (*gfr)->headerarg = headerarg;
}

void gfc_set_writefunc(gfcrequest_t **gfr, void (*writefunc)(void *, size_t, void *)) {
  (*gfr)->writefunc = writefunc;
}

const char *gfc_strstatus(gfstatus_t status) {
  const char *strstatus = "UNKNOWN";

  switch (status) {

    case GF_OK: {
      strstatus = "OK";
    } break;

    case GF_FILE_NOT_FOUND: {
      strstatus = "FILE_NOT_FOUND";
    } break;

   case GF_INVALID: {
      strstatus = "INVALID";
    } break;
   
   case GF_ERROR: {
      strstatus = "ERROR";
    } break;

  }

  return strstatus;
}
