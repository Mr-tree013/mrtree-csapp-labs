#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mm.h"

/*
 * mm_init - Initialize the memory allocator
 * Returns 0 on success, -1 on error
 */
int mm_init(void) {
  // TODO: initialize the heap and free list
  return 0;
}

/*
 * mm_malloc - Allocate a block of at least `size` bytes
 * Returns pointer to allocated block, NULL on failure
 */
void *mm_malloc(size_t size) {
  // TODO: implement allocation (first-fit, next-fit, or best-fit)
  return NULL;
}

/*
 * mm_free - Free a previously allocated block
 */
void mm_free(void *ptr) {
  // TODO: mark block as free, coalesce with adjacent free blocks
}

/*
 * mm_realloc - Resize an allocated block to `size` bytes
 * Returns pointer to new block, NULL on failure
 */
void *mm_realloc(void *ptr, size_t size) {
  // TODO: implement reallocation
  return NULL;
}
