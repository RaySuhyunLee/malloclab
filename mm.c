/*
 * mm-naive.c - The fastest, least memory-efficient malloc package.
 *
 * In this naive approach, a block is allocated by simply incrementing
 * the brk pointer.  A block is pure payload. There are no headers or
 * footers.  Blocks are never coalesced or reused. Realloc is
 * implemented directly using mm_malloc and mm_free.
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your information in the following struct.
 ********************************************************/
team_t team = {
  /* Team name : Your student ID */
  "2013-11419",
  /* Your full name */
  "Suhyun Lee",
  /* Your student ID */
  "2013-11419",
  /* leave blank */
  "",
  /* leave blank */
  ""
};

/* DON'T MODIFY THIS VALUE AND LEAVE IT AS IT WAS */
static range_t **gl_ranges;

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)


#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))


/* my implementations */
static void* cursor;

#define IS_ALLOCATED(header) ((header)&0x1)
#define GET_SIZE(header) ((header) & ~0x7)

//#define DEBUG

/* 
 * remove_range - manipulate range lists
 * DON'T MODIFY THIS FUNCTION AND LEAVE IT AS IT WAS
 */
static void remove_range(range_t **ranges, char *lo)
{
  range_t *p;
  range_t **prevpp = ranges;
  
  if (!ranges)
    return;

  for (p = *ranges;  p != NULL; p = p->next) {
    if (p->lo == lo) {
      *prevpp = p->next;
      free(p);
      break;
    }
    prevpp = &(p->next);
  }
}

/*
 * mm_init - initialize the malloc package.
 */
int mm_init(range_t **ranges)
{
  /* YOUR IMPLEMENTATION */
	cursor = NULL;

  /* DON't MODIFY THIS STAGE AND LEAVE IT AS IT WAS */
  gl_ranges = ranges;

  return 0;
}

/*
 * mm_malloc - Allocate a block by searching for a free block or incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void* mm_malloc(size_t size)
{
  int newsize = ALIGN(size + SIZE_T_SIZE);
	void* p;
	size_t header;
	size_t oldsize;
	int diff;

	// next fit
	if (cursor != NULL) p = cursor;
	else								p = mem_heap_lo();

	if (mem_heap_hi() - mem_heap_lo() + 1 >= newsize) {
		while(p < mem_heap_hi()) {
			header = *(size_t*)p;
			if (IS_ALLOCATED(header))
				p += GET_SIZE(header);
			else if ((oldsize = GET_SIZE(header)) >= newsize) {
				*(size_t*)p = newsize + 0x1;
				// split blocks
				if ((diff = oldsize - newsize) > 0) {
					*(size_t*)(p+newsize) = diff;
				}
#ifdef DEBUG
				printf("%d bytes allocated with p: %p\n", GET_SIZE(*(size_t*)p), p);
#endif
				cursor = p;
				return p + SIZE_T_SIZE;
			}
			else
				p += GET_SIZE(header);
		}
	}

	// if there isn't enough space
  p = mem_sbrk(newsize);

  if (p == (void *)-1)
    return NULL;
  else {
    *(size_t *)p = newsize + 0x1;
#ifdef DEBUG
		printf("%d bytes allocated with sbrk with p: %p\n", GET_SIZE(*(size_t*)p), p);
#endif
		cursor = p;
    return (void *)((char *)p + SIZE_T_SIZE);
  }
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr)
{
	void *header_ptr = ptr - SIZE_T_SIZE;
	size_t header = *(size_t*)header_ptr;
	void *next_ptr = header_ptr + GET_SIZE(header);
	size_t header_next = *(size_t*)next_ptr;
	if (IS_ALLOCATED(header)) {
		// check if coalescing is available
		if (next_ptr > mem_heap_hi() || IS_ALLOCATED(header_next)) {
			*(size_t*)header_ptr -= 0x1;
		} else {
			*(size_t*)header_ptr = GET_SIZE(header) + GET_SIZE(header_next);
		}
		cursor = header_ptr;
#ifdef DEBUG
		printf("%d bytes freed with ptr: %p\n", GET_SIZE(*(size_t*)header_ptr), header_ptr);
#endif
	}
	else {
#ifdef DEBUG
		printf("already freed at ptr: %p\n", header_ptr);
#endif
		return;
	}

  /* DON't MODIFY THIS STAGE AND LEAVE IT AS IT WAS */
  if (gl_ranges)
    remove_range(gl_ranges, ptr);
}

/*
 * mm_realloc - empty implementation; YOU DO NOT NEED TO IMPLEMENT THIS
 */
void* mm_realloc(void *ptr, size_t t)
{
  return NULL;
}

/*
 * mm_exit - finalize the malloc package.
 */
void mm_exit(void)
{
#ifdef DEBUG
	printf("exit\n");
#endif
	void* p;
	for(p=mem_heap_lo(); p<=mem_heap_hi(); p+=GET_SIZE(*(size_t*)p))
		mm_free(p+SIZE_T_SIZE);
}

