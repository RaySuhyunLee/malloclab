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
static void* free_first;
static void* free_last;

/* block structure */
/* ========== */
/* | header | */
/* | next   | */
/* | prev   | */
/* | ...    | */
/* | footer | */
/* ========== */
#define MIN_SIZE ((ALIGNMENT<<1) + (SIZE_T_SIZE<<1))
#define PREV_OFFSET SIZE_T_SIZE
#define NEXT_OFFSET (SIZE_T_SIZE + ALIGNMENT)
#define GET_HEADER(ptr) (*(size_t*)(ptr))
#define PREV(ptr) (*(void**)((ptr) + PREV_OFFSET))
#define NEXT(ptr) (*(void**)((ptr) + NEXT_OFFSET))
#define IS_ALLOCATED(header) ((header)&0x1)
#define GET_SIZE(header) ((header) & ~0x7)

/* for segregated list */
#define DETACH(ptr, index) \
	if (PREV(ptr) == NULL) GET_FIRST(index) = NEXT(ptr);	/* if this is the first free block */ \
	else							NEXT(PREV(ptr)) = NEXT(ptr); \
	if (NEXT(ptr) == NULL) GET_LAST(index) = PREV(ptr);		/* if this is the last free block */ \
	else							PREV(NEXT(ptr)) = PREV(ptr);
#define SEGLIST_NUM 14
#define GET_FIRST(index) (*(void**)(mem_heap_lo() + (index)))
#define GET_LAST(index) (*(void**)(mem_heap_lo() + SEGLIST_NUM + (index)))

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
	void* p;	
	p = mem_sbrk(SEGLIST_NUM<<1);

	if (p == (void *)-1)
		return 0;
	else {
		for (; p<=mem_heap_hi(); p++)
			*(void**)p = NULL;
	}

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
  int newsize = ALIGN(size) > (ALIGNMENT<<1)?
		ALIGN(size) + (SIZE_T_SIZE<<1) : (ALIGNMENT<<1) + (SIZE_T_SIZE<<1);
	void* p;
	void* split;
	void* next;
	void* prev;
	size_t header;
	size_t headersize;
	int diff;
	int i;

	void* first;
	void* last;
		
	/* free list is not empty and heap size is big enough */
	if (mem_heap_hi() - mem_heap_lo() + 1 >= newsize) {
		for (i=SEGLIST_NUM; i>0; i--)
			if ((newsize>>i) & 0x1) break;
		first = GET_FIRST(i-1);
		last = GET_LAST(i-1);
		p = first;
		while(p != NULL) {
			header = GET_HEADER(p);
			headersize = GET_SIZE(header);
			next = NEXT(p);
			/* if size is big enough */
			if (headersize >= newsize) {
#ifdef DEBUG
				printf("req: %d | Found a proper block of size %d at %p\n", size, headersize, p);
#endif
				/* write header */
				GET_HEADER(p) = newsize + 0x1;
				GET_HEADER(p+newsize - SIZE_T_SIZE) = newsize + 0x1;
				prev = PREV(p);
				/* split blocks */
				if ((diff = headersize - newsize) > 0) {
					split = p + newsize;
#ifdef DEBUG
					printf("split at %p with diff: %d\n", split, diff);
#endif
					/* write header */
					GET_HEADER(split) = diff;
					GET_HEADER(split + diff - SIZE_T_SIZE) = diff;
				}
#ifdef DEBUG
				printf("before: first: %p | last: %p | prev: %p | next: %p\n", first, last, prev, next);
#endif
				/* write free pointer */
				if (diff >= MIN_SIZE) {
					if (prev == NULL) GET_FIRST(i-1) = split;
					else							NEXT(prev) = split;
					if (next == NULL) GET_LAST(i-1) = split;
					else							PREV(next) = split;
					NEXT(split) = next;
					PREV(split) = prev;
				} else {
					DETACH(p, i-1)
				}
#ifdef DEBUG
				printf("after: free_first: %p | free_last: %p | prev: %p | next: %p\n", free_first, free_last, prev, next);
				printf("req: %d | actual: %d bytes allocated with p: %p\n\n", size, GET_SIZE(GET_HEADER(p)), p);
#endif
				return p + SIZE_T_SIZE;
			}
			/* if size is not big enough */
			else { 
				p = next;
			}
		}
	}  

	/* if there isn't enough space */
	p = mem_sbrk(newsize);

	if (p == (void *)-1)
		return NULL;
	else {
		GET_HEADER(p) = newsize + 0x1;
		GET_HEADER(p + newsize - SIZE_T_SIZE) = newsize + 0x1;

#ifdef DEBUG
		printf("req: %d | actual: %d bytes allocated with sbrk with p: %p\n\n", size, GET_SIZE(GET_HEADER(p)), p);
#endif
		return p + SIZE_T_SIZE;
	}
}
	
/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr)
{
	void *header_ptr = ptr - SIZE_T_SIZE;
	size_t header = GET_HEADER(header_ptr);
	size_t headersize = GET_SIZE(header);

	void *next_ptr = header_ptr + GET_SIZE(header);
	size_t header_next = GET_HEADER(next_ptr);
	size_t nextsize = GET_SIZE(header_next);

	void *prev_ptr = header_ptr - GET_SIZE(GET_HEADER(header_ptr - SIZE_T_SIZE));
	size_t header_prev = GET_HEADER(prev_ptr);
	size_t prevsize = GET_SIZE(header_prev);

	int next_coal;
	int prev_coal;

	void* first;
	void* last;
	int i;

#ifdef DEBUG
	printf("free called with ptr: %p, header_ptr: %p, header: %d, allocated: %d\n", ptr, header_ptr, header, IS_ALLOCATED(header));
	printf("before: free_first: %p | free_last: %p\n", free_first, free_last);
#endif
	if (IS_ALLOCATED(header)) {

		/* check if coalescing is possible */

		next_coal = next_ptr <= mem_heap_hi() && !IS_ALLOCATED(header_next);
		prev_coal = prev_ptr >= mem_heap_lo() && !IS_ALLOCATED(header_prev); 
		if (next_coal && prev_coal) {
			GET_HEADER(prev_ptr) = headersize + prevsize + nextsize;
			GET_HEADER(next_ptr + nextsize - SIZE_T_SIZE) = headersize + prevsize + nextsize;
			header_ptr = prev_ptr;
			
			/* detach link */
			for (i=SEGLIST_NUM; i>0; i--)
				if ((prevsize>>i) & 0x1) break;
			if (prevsize >= MIN_SIZE) {
				DETACH(prev_ptr, i-1)
			}
			for (i=SEGLIST_NUM; i>0; i--)
				if ((nextsize>>i) & 0x1) break;
			if (nextsize >= MIN_SIZE) {
				DETACH(next_ptr, i-1)
			}
		}
		/* if next block is free */
		else if (next_coal) {
			GET_HEADER(header_ptr) = headersize + nextsize;
			GET_HEADER(next_ptr + nextsize - SIZE_T_SIZE) = headersize + nextsize;
			
			/* detach link */
			for (i=SEGLIST_NUM; i>0; i--)
				if ((nextsize>>i) & 0x1) break;
			if (nextsize >= MIN_SIZE) {
				DETACH(next_ptr, i-1)
			}
		}
		/* if previous block is free */
		else if (prev_coal) {
			GET_HEADER(prev_ptr) = headersize + prevsize;
			GET_HEADER(next_ptr - SIZE_T_SIZE) = headersize + prevsize;
			header_ptr = prev_ptr;

			/* detach link */
			for (i=SEGLIST_NUM; i>0; i--)
				if ((prevsize>>i) & 0x1) break;
			if (prevsize >= MIN_SIZE) {
				DETACH(prev_ptr, i-1)
			}
		}
		/* no other blocks are free */
		else {
			GET_HEADER(header_ptr) -= 0x1;
			GET_HEADER(next_ptr - SIZE_T_SIZE) -= 0x1;
		}
		for (i=SEGLIST_NUM; i>0; i--)
			if ((GET_SIZE(GET_HEADER(header_ptr))>>i) & 0x1) break;
		first = GET_FIRST(i-1);
		last = GET_LAST(i-1);

		/* write free pointer */
		if (last == NULL) {
			GET_FIRST(i-1) = header_ptr;
			GET_LAST(i-1) = header_ptr;
			NEXT(header_ptr) = NULL;
			PREV(header_ptr) = NULL;
		} else {
			PREV(header_ptr) = last;
			NEXT(header_ptr) = NULL;
			GET_LAST(i-1) = header_ptr;
		}
#ifdef DEBUG
		printf("after: free_first: %p | free_last: %p\n", free_first, free_last);
		printf("%d bytes freed with ptr: %p\n\n", GET_SIZE(GET_HEADER(header_ptr)), header_ptr);
#endif
	}
	else {
#ifdef DEBUG
		printf("already freed at ptr: %p\n\n", header_ptr);
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

