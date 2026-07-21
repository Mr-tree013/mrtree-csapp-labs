#ifndef CSIM_H
#define CSIM_H

typedef struct {
  int valid;
  unsigned long tag;
  int lru_counter;  /* 用于 LRU 替换 */
} cache_line_t;

typedef struct {
  cache_line_t *lines;
} cache_set_t;

typedef struct {
  int s;             /* set 索引位数 */
  int E;             /* 关联度（每个 set 的 line 数） */
  int b;             /* block 偏移位数 */
  int S;             /* set 数量 = 2^s */
  cache_set_t *sets;
} cache_t;

cache_t *cache_init(int s, int E, int b);
void cache_free(cache_t *cache);
int cache_access(cache_t *cache, unsigned long addr);

#endif
