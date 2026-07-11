#ifndef CSIM_H
#define CSIM_H

typedef struct {
  int valid;
  unsigned long tag;
  int lru_counter;  /* for LRU replacement */
} cache_line_t;

typedef struct {
  cache_line_t *lines;
} cache_set_t;

typedef struct {
  int s;             /* set index bits */
  int E;             /* associativity (lines per set) */
  int b;             /* block offset bits */
  int S;             /* number of sets = 2^s */
  cache_set_t *sets;
} cache_t;

cache_t *cache_init(int s, int E, int b);
void cache_free(cache_t *cache);
int cache_access(cache_t *cache, unsigned long addr);

#endif
