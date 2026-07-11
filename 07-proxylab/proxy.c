#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>

#define MAXLINE  8192  /* max line size */
#define MAX_CACHE_SIZE (1024 * 1024)  /* 1 MB cache */
#define MAX_OBJECT_SIZE (100 * 1024)  /* 100 KB max object */

/* Cache entry structure */
typedef struct {
  char   uri[MAXLINE];
  char  *data;
  size_t size;
  /* for LRU */ int lru_counter;
} cache_entry_t;

/*
 * proxy main: proxy <port>
 */
int main(int argc, char **argv) {
  // TODO: parse port, set up listening socket, accept connections
  printf("TODO: HTTP proxy\n");
  return 0;
}

/*
 * handle_client - Read HTTP request from client, forward to server, return response
 */
void *handle_client(void *arg) {
  // TODO: read request, parse URI, forward, cache
  return NULL;
}

/*
 * parse_uri - Parse HTTP URI into hostname, port, and path
 */
int parse_uri(char *uri, char *hostname, char *port, char *path) {
  // TODO: extract hostname/port/path from URI
  return -1;
}

/*
 * cache_lookup - Search cache for a URI, return cached data or NULL
 */
char *cache_lookup(char *uri, size_t *size) {
  // TODO: check cache
  return NULL;
}

/*
 * cache_store - Insert object into cache, evict using LRU if needed
 */
void cache_store(char *uri, char *data, size_t size) {
  // TODO: store in cache
}
