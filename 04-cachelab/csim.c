#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "csim.h"

cache_t *cache_init(int s, int E, int b) {
  // TODO: allocate and initialize cache structure
  return NULL;
}

void cache_free(cache_t *cache) {
  // TODO: free all allocated memory
}

int cache_access(cache_t *cache, unsigned long addr) {
  // TODO: simulate cache access, return 1 for hit, 0 for miss
  // implement LRU replacement
  return 0;
}

int main(int argc, char *argv[]) {
  // TODO: parse command-line args (s, E, b, tracefile)
  // TODO: read trace file and simulate cache
  // TODO: print hits, misses, evictions
  printf("TODO: cache simulator\n");
  return 0;
}
