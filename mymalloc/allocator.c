/**
 * Copyright (c) 2015 MIT License by 6.172 Staff
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 **/

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "./allocator_interface.h"
#include "./memlib.h"

// Don't call libc malloc!
#define malloc(...) (USE_MY_MALLOC)
#define free(...) (USE_MY_FREE)
#define realloc(...) (USE_MY_REALLOC)

// All blocks must have a specified minimum alignment.
// The alignment requirement (from config.h) is >= 8 bytes.
#ifndef ALIGNMENT
  #define ALIGNMENT 8
#endif

// Rounds up to the nearest multiple of ALIGNMENT.
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~(ALIGNMENT-1))

// The smallest aligned size that will hold a size_t value.
#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

// The smallest aligned size that will hold a unsigned int value.
#define UINT_SIZE (ALIGN(sizeof(unsigned int)))

#define HEADER_SIZE (ALIGN(SIZE_T_SIZE + UINT_SIZE))

#define BLOCK_SIZE(idx) ALIGN((1 << (idx + 3)))

// The bin size including header
#define BIN_SIZE(idx) ALIGN((BLOCK_SIZE(idx) + HEADER_SIZE))

#define BINNED_LIST_SIZE 20

#define MAX(x, y) ((x > y) ? x : y)

/* ------------------------------------------------------------------------- */
//
// HELPER FUNCTIONS
//
/* ------------------------------------------------------------------------- */

size_t GET_ALL_BIN_SIZE() {
  size_t ret = 0;
  for (int i = 0; i < BINNED_LIST_SIZE; i++) {
    ret += BIN_SIZE(i);
  }
  return ret;
}

void SET_BLOCK_NEXT(void *ptr, size_t next) {
  *(size_t *)ptr = next;
}

void SET_BLOCK_SIZE(void *ptr, unsigned int size) {
  char *p = (char *)ptr + SIZE_T_SIZE;
  *(unsigned int *)p = size;
}

size_t GET_BLOCK_NEXT(void *ptr) {
  return *(size_t *)ptr;
}

unsigned int GET_BLOCK_SIZE(void *ptr) {
  char *p = (char *)ptr + SIZE_T_SIZE;
  return *(unsigned int *)p;
}

// When inceasing heap size, just add one new mem block into each bin
void increase_heap_size() {
  unsigned int incr = GET_ALL_BIN_SIZE();

  char *p = (char *) mem_sbrk(incr);
  char *bfl = (char *) mem_heap_lo();
  size_t tmp;

  for (int i = 0; i < BINNED_LIST_SIZE; i++) {
    tmp = GET_BLOCK_NEXT((void *) bfl);
    
    SET_BLOCK_NEXT(bfl, (size_t) p);
    SET_BLOCK_NEXT((void *) p, tmp);
    SET_BLOCK_SIZE((void *) p, BLOCK_SIZE(i));

    p += BIN_SIZE(i);
    bfl += SIZE_T_SIZE;
  }
  /* my_check(); */
}

// Divide block l into smaller blocks, return a block of size k
void *divide_block(int k, int l) {
  char *bfl = (char *) mem_heap_lo() + (k * SIZE_T_SIZE);
  size_t mem_block = GET_BLOCK_NEXT(((char *) mem_heap_lo() + (l * SIZE_T_SIZE)));

  // Set bfl to point to next block
  size_t next_block = GET_BLOCK_NEXT((void *) mem_block);
  char *divided_block_list = (char *) mem_heap_lo() + (l * SIZE_T_SIZE);
  SET_BLOCK_NEXT((void *)divided_block_list, next_block);

  void *ret = (void *) mem_block;
  SET_BLOCK_SIZE(ret, BLOCK_SIZE(k));
  SET_BLOCK_NEXT(ret, 0);
  mem_block += BIN_SIZE(k);

  size_t tmp;
  for (int i = k; i < l; i++) {
    tmp = GET_BLOCK_NEXT((void *) bfl);

    SET_BLOCK_NEXT(bfl, mem_block);
    SET_BLOCK_SIZE((void *) mem_block, BLOCK_SIZE(i));
    SET_BLOCK_NEXT((void *) mem_block, tmp);

    mem_block += BIN_SIZE(i);
  }
  return ret;
}

// check - This checks our invariant that the size_t header before every
// block points to either the beginning of the next block, or the end of the
// heap.
int my_check() {
  char *p;
  char *lo = (char *)mem_heap_lo() + (SIZE_T_SIZE * BINNED_LIST_SIZE);
  char *hi = (char *)mem_heap_hi() + 1;
  size_t size;

  p = lo;
  while(lo <= p && p < hi) {
    size = GET_BLOCK_SIZE((void *) p);
    p += ALIGN(HEADER_SIZE + size);
  }

  if (p != hi) {
    printf("Bad headers did not end at heap_hi!\n");
    printf("heap_lo: %p, heap_hi: %p, size: %lu, p: %p\n", lo, hi, size, p);
    return -1;
  }

  return 0;
}

// init - Initialize the malloc package.  Called once before any other
// calls are made.  Since this is a very simple implementation, we just
// return success.
int my_init() {
  size_t initial_heap_size = GET_ALL_BIN_SIZE() + SIZE_T_SIZE * BINNED_LIST_SIZE;

  char *p = mem_sbrk(ALIGN(initial_heap_size));
  if (p == (void *) - 1) {
    return -1;
  }

  char *heap_pos = (char *)p + (SIZE_T_SIZE * BINNED_LIST_SIZE);
  for (int i = 0; i < BINNED_LIST_SIZE; i++) {
    SET_BLOCK_NEXT(p, (size_t) heap_pos);
    SET_BLOCK_NEXT((void *) heap_pos, 0);
    SET_BLOCK_SIZE((void *) heap_pos, BLOCK_SIZE(i));

    heap_pos += BIN_SIZE(i);
    p += SIZE_T_SIZE;
  }

  /* my_check(); */
  return 0;
}

//  malloc - Allocate a block by incrementing the brk pointer.
//  Always allocate a block whose size is a multiple of the alignment.
void* my_malloc(size_t size) {
  // Find the first bin size that can fit our size
  double num = MAX(0, log2(size) - 3);
  int result = ceil(num);

  // Allocate the the first block at this bin.
  // If there isn't a block available, then try breaking up a larger
  //    block or allocate more heap memory.
  // If there is a block available, then we can simply allocate that block.
  char *list = (char *) mem_heap_lo() + (result * SIZE_T_SIZE);

  char *ptr = list;
  int j = result;
  while(j < BINNED_LIST_SIZE && GET_BLOCK_NEXT(ptr) == 0) {
    ptr += SIZE_T_SIZE;
    j++;
  }

  if (j == BINNED_LIST_SIZE) {
    increase_heap_size();
    j = result; // set j = i since there is now a free block at that size.
  }

  void *p;
  if (j == result) {
    // Allocate the first block in this list
    p = (void *) (GET_BLOCK_NEXT(list));
    SET_BLOCK_NEXT((void *) list, GET_BLOCK_NEXT(p));
    SET_BLOCK_NEXT(p, 0);
    my_check();
  } else {
    // break up block, then allocate the first block
    p = divide_block(result, j);
  }

  /* my_check(); */
  return p;
}

// Freed block is inserted back into the bfl.
void my_free(void* ptr) {

  // First, get the bin this block belongs to.
  double num = log2(GET_BLOCK_SIZE(ptr));
  int result = ceil(num);
  
  char *bfl = (char *) mem_heap_lo() + (result * SIZE_T_SIZE);
  size_t head = GET_BLOCK_NEXT((void *) bfl);
  SET_BLOCK_NEXT((void *) bfl, (size_t) ptr);
  SET_BLOCK_NEXT(ptr, head);
  /* my_check(); */
}

// realloc - Implemented simply in terms of malloc and free
void* my_realloc(void* ptr, size_t size) {
  void* newptr;
  size_t copy_size;

  // Allocate a new chunk of memory, and fail if that allocation fails.
  newptr = my_malloc(size);
  if (NULL == newptr) {
    return NULL;
  }

  // Get the size of the old block of memory.  Take a peek at my_malloc(),
  // where we stashed this in the SIZE_T_SIZE bytes directly before the
  // address we returned.  Now we can back up by that many bytes and read
  // the size.
  copy_size = *(size_t*)((uint8_t*)ptr - SIZE_T_SIZE);

  // If the new block is smaller than the old one, we have to stop copying
  // early so that we don't write off the end of the new block of memory.
  if (size < copy_size) {
    copy_size = size;
  }

  // This is a standard library call that performs a simple memory copy.
  memcpy(newptr, ptr, copy_size);

  // Release the old block.
  my_free(ptr);

  // Return a pointer to the new block.
  return newptr;
}
