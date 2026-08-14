#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include "cachelab.h"
#include "csim.h"

/* LRU 全局时钟：每次访问递增，行内 lru_counter 越大表示最近越常被访问 */
static int timestamp = 0;

cache_t *cache_init(int s, int E, int b)
{
    cache_t *cache = malloc(sizeof(cache_t));
    if (cache == NULL)
        return NULL;

    cache->s = s;
    cache->E = E;
    cache->b = b;
    cache->S = 1 << s;

    cache->sets = calloc(cache->S, sizeof(cache_set_t));
    if (cache->sets == NULL) {
        free(cache);
        return NULL;
    }

    for (int i = 0; i < cache->S; i++) {
        cache->sets[i].lines = calloc(E, sizeof(cache_line_t));
        if (cache->sets[i].lines == NULL) {
            for (int j = 0; j < i; j++)
                free(cache->sets[j].lines);
            free(cache->sets);
            free(cache);
            return NULL;
        }
    }

    return cache;
}

void cache_free(cache_t *cache)
{
    for (int i = 0; i < cache->S; i++)
        free(cache->sets[i].lines);
    free(cache->sets);
    free(cache);
}

/* 返回 1 命中；0 未命中无驱逐；-1 未命中且发生驱逐 */
int cache_access(cache_t *cache, unsigned long addr)
{
    unsigned long tag = addr >> (cache->s + cache->b);
    unsigned long mask = (1UL << cache->s) - 1;
    unsigned int set_idx = (addr >> cache->b) & mask;
    cache_set_t *set = &cache->sets[set_idx];

    timestamp++;

    for (int i = 0; i < cache->E; i++) {
        if (set->lines[i].valid && set->lines[i].tag == tag) {
            set->lines[i].lru_counter = timestamp;
            return 1;
        }
    }

    int victim = -1;
    for (int i = 0; i < cache->E; i++) {
        if (!set->lines[i].valid) {
            victim = i;
            break;
        }
    }
    if (victim == -1) {
        victim = 0;
        for (int i = 1; i < cache->E; i++)
            if (set->lines[i].lru_counter < set->lines[victim].lru_counter)
                victim = i;
    }

    int eviction = set->lines[victim].valid;
    set->lines[victim].valid = 1;
    set->lines[victim].tag = tag;
    set->lines[victim].lru_counter = timestamp;

    return eviction ? -1 : 0;
}

static void do_access(cache_t *cache, unsigned long addr, int verbose,
                      int *hits, int *misses, int *evictions)
{
    int result = cache_access(cache, addr);

    if (result == 1) {
        (*hits)++;
        if (verbose)
            printf(" hit");
    } else {
        (*misses)++;
        if (result == -1) {
            (*evictions)++;
            if (verbose)
                printf(" miss eviction");
        } else if (verbose) {
            printf(" miss");
        }
    }
}

static void usage(const char *prog)
{
    printf("Usage: %s [-hv] -s <num> -E <num> -b <num> -t <file>\n", prog);
    printf("Options:\n");
    printf("  -h         Print this help message.\n");
    printf("  -v         Optional verbose flag.\n");
    printf("  -s <num>   Number of set index bits.\n");
    printf("  -E <num>   Number of lines per set.\n");
    printf("  -b <num>   Number of block offset bits.\n");
    printf("  -t <file>  Trace file.\n");
}

int main(int argc, char *argv[])
{
    int s = 0, E = 0, b = 0, verbose = 0;
    char *tracefile = NULL;
    int opt;

    while ((opt = getopt(argc, argv, "hvs:E:b:t:")) != -1) {
        switch (opt) {
        case 's': s = atoi(optarg); break;
        case 'E': E = atoi(optarg); break;
        case 'b': b = atoi(optarg); break;
        case 't': tracefile = optarg; break;
        case 'v': verbose = 1; break;
        case 'h':
            usage(argv[0]);
            return 0;
        default:
            usage(argv[0]);
            return 1;
        }
    }

    if (tracefile == NULL || s <= 0 || E <= 0 || b <= 0) {
        fprintf(stderr, "%s: Missing required command line argument\n", argv[0]);
        usage(argv[0]);
        return 1;
    }

    cache_t *cache = cache_init(s, E, b);
    if (cache == NULL) {
        fprintf(stderr, "Error: failed to allocate cache\n");
        return 1;
    }

    FILE *fp = fopen(tracefile, "r");
    if (fp == NULL) {
        fprintf(stderr, "Error: cannot open trace file %s\n", tracefile);
        cache_free(cache);
        return 1;
    }

    int hits = 0, misses = 0, evictions = 0;
    char op;
    unsigned long addr;
    int size;

    while (fscanf(fp, " %c %lx,%d", &op, &addr, &size) == 3) {
        if (op == 'I')
            continue;

        if (verbose)
            printf("%c %lx,%d", op, addr, size);

        if (op == 'L' || op == 'S') {
            do_access(cache, addr, verbose, &hits, &misses, &evictions);
        } else if (op == 'M') {
            do_access(cache, addr, verbose, &hits, &misses, &evictions);
            do_access(cache, addr, verbose, &hits, &misses, &evictions);
        }

        if (verbose)
            printf(" \n");
    }

    fclose(fp);
    cache_free(cache);
    printSummary(hits, misses, evictions);
    return 0;
}
