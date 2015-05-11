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
#define SEGLIST_NUM 16
#define GET_FIRST(index) (*((void**)mem_heap_lo() + (index)))
#define DETACH(ptr, index) \
	if (PREV(ptr) == NULL) GET_FIRST(index) = NEXT(ptr);	/* if this is the first free block */ \
	else									NEXT(PREV(ptr)) = NEXT(ptr); \
	if (NEXT(ptr) != NULL) PREV(NEXT(ptr)) = PREV(ptr);
#define GET_INDEX(size, var) \
		for ((var)=0; (var)<SEGLIST_NUM-1; (var)++) \
			if ((0x1<<(var)) < (size) && (0x1<<((var)+1)) >= (size)) break;


static void* heap_start;

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
 * mm_check
 */
void mm_check(void) {
	void** p;
	int i=0;
	for(p=mem_heap_lo(); p<(void**)mem_heap_lo()+ALIGN(SEGLIST_NUM); p++, i++)
		printf("[%d: %p] ", i, *p);
	printf("\n");
}

/*
 * mm_init - initialize the malloc package.
 */
int mm_init(range_t **ranges)
{
  /* YOUR IMPLEMENTATION */
	/* allocate special memory for seglist */
	void* p;	
	p = mem_sbrk(ALIGN((SEGLIST_NUM)*sizeof(void**)));
	if (p == (void *)-1)
		return 0;
	else {
		heap_start = mem_heap_hi() + 1;
		for (; p<=mem_heap_hi(); p++)
			*(void**)p = NULL;
	}
#ifdef DEBUG
	mm_check();
	printf("initialized\n\n");
#endif

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
	
#ifdef DEBUG
	printf("malloc called with req: %d\n", size);
	//if (size == 26940) exit(0);
#endif
	/* free list is not empty and heap size is big enough */
	if (mem_heap_hi() - heap_start + 1 >= newsize) {
		GET_INDEX(newsize, i)
		first = GET_FIRST(i);
		p = first;
		while(i<SEGLIST_NUM) {
			if (p == NULL) {
				i++;
				first = GET_FIRST(i);
				p = first;
				continue;
			}
			header = GET_HEADER(p);
			headersize = GET_SIZE(header);
			next = NEXT(p);
#ifdef DEBUG
			printf("i: %d, p: %p, next: %p\n", i, p, next);
#endif
			/* if size is big enough */
			if (headersize >= newsize) {
#ifdef DEBUG
				printf("req: %d | Found a proper block of size %d at %p, index: %d\n", size, headersize, p, i);
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
				//printf("before: first: %p | prev: %p | next: %p\n", first, prev, next);
#endif
				/* write free pointer */
				if (diff >= MIN_SIZE) {
#ifdef DEBUG
					printf("detach: curr: %p, prev: %p, next: %p\n", p, PREV(p), NEXT(p));
#endif
					DETACH(p, i)
#ifdef DEBUG
					printf("detached %p from index:%d\n", p, i);
#endif

					GET_INDEX(diff, i)
					if (GET_FIRST(i) == NULL) {
						GET_FIRST(i) = split;
						NEXT(split) = NULL;
						PREV(split) = NULL;
					} else {
						NEXT(split) = GET_FIRST(i);
						PREV(split) = NULL;
						PREV(GET_FIRST(i)) = split;
						GET_FIRST(i) = split;
					}
#ifdef DEBUG
					printf("attached %p in index:%d\n", split, i);
#endif
				} else {
					DETACH(p, i)
				}
#ifdef DEBUG
				//printf("after: first: %p | prev: %p | next: %p\n", GET_FIRST(i), prev, next);
				mm_check();
				printf("req: %d | actual: %d bytes allocated with p: %p\n\n", size, GET_SIZE(GET_HEADER(p)), p);
#endif
				return p + SIZE_T_SIZE;
			}
			/* if size is not big enough */
			else {
				p = next;
			}
		}
#ifdef DEBUG
		printf("Could not find a proper block\n");
#endif
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
	size_t header;
	size_t headersize;

	void *next_ptr; 
	size_t header_next;
	size_t nextsize;

	void *prev_ptr;
	size_t header_prev;
	size_t prevsize;

	int next_coal;
	int prev_coal;

	void* first;
	int i;

	if (header_ptr < heap_start || header_ptr > mem_heap_hi()) {
		printf("free: Invalid address!\n");
		exit(1);
	}
	header = GET_HEADER(header_ptr);
	headersize = GET_SIZE(header);
	if (IS_ALLOCATED(header)) {
#ifdef DEBUG
		printf("free called with header_ptr: %p, header: %d\n", header_ptr, header);
#endif
		/* initialize variables */
		next_ptr = header_ptr + GET_SIZE(header);
		prev_ptr = header_ptr > heap_start?
			header_ptr - GET_SIZE(GET_HEADER(header_ptr - SIZE_T_SIZE)) : 0;
		/* check if coalescing is possible */
		next_coal = next_ptr <= mem_heap_hi() && !IS_ALLOCATED(header_next = GET_HEADER(next_ptr));
		prev_coal = prev_ptr >= heap_start && !IS_ALLOCATED(header_prev = GET_HEADER(prev_ptr));
		nextsize = next_coal? GET_SIZE(header_next) : 0;
		prevsize = prev_coal? GET_SIZE(header_prev) : 0;

		if (next_coal && prev_coal) {
			GET_HEADER(prev_ptr) = headersize + prevsize + nextsize;
			GET_HEADER(next_ptr + nextsize - SIZE_T_SIZE) = headersize + prevsize + nextsize;
			header_ptr = prev_ptr;
			
			/* detach link */
			if (prevsize >= MIN_SIZE) {
				GET_INDEX(prevsize, i)
				DETACH(prev_ptr, i)
#ifdef DEBUG
				printf("coalescing with prev: detached %p in %d\n", prev_ptr, i);
#endif
			}
			if (nextsize >= MIN_SIZE) {
				GET_INDEX(nextsize, i)
				DETACH(next_ptr, i)
#ifdef DEBUG
				printf("coalescing with next: detached %p in %d\n", next_ptr, i);
#endif
			}
		}
		/* if next block is free */
		else if (next_coal) {
			GET_HEADER(header_ptr) = headersize + nextsize;
			GET_HEADER(next_ptr + nextsize - SIZE_T_SIZE) = headersize + nextsize;
			
			/* detach link */
			if (nextsize >= MIN_SIZE) {
				GET_INDEX(nextsize, i)
				DETACH(next_ptr, i)
#ifdef DEBUG
				printf("coalescing with next: detached %p in %d\n", next_ptr, i);
#endif
			}
		}
		/* if previous block is free */
		else if (prev_coal) {
			GET_HEADER(prev_ptr) = headersize + prevsize;
			GET_HEADER(next_ptr - SIZE_T_SIZE) = headersize + prevsize;
			header_ptr = prev_ptr;

			/* detach link */
			if (prevsize >= MIN_SIZE) {
				GET_INDEX(prevsize, i)
				DETACH(prev_ptr, i)
#ifdef DEBUG
				printf("coalescing with prev: detached %p in %d\n", prev_ptr, i);
#endif
			}
		}
		/* no other blocks are free */
		else {
			GET_HEADER(header_ptr) -= 0x1;
			GET_HEADER(next_ptr - SIZE_T_SIZE) -= 0x1;
#ifdef DEBUG
			printf("no coalescing\n");
#endif
		}
		GET_INDEX(GET_SIZE(GET_HEADER(header_ptr)), i)
		first = GET_FIRST(i);

		/* write free pointer */
		if (first == NULL) {
			GET_FIRST(i) = header_ptr;
			NEXT(header_ptr) = NULL;
			PREV(header_ptr) = NULL;
		} else {
			NEXT(header_ptr) = GET_FIRST(i);
			PREV(header_ptr) = NULL;
			PREV(GET_FIRST(i)) = header_ptr;
			GET_FIRST(i) = header_ptr;
		}
#ifdef DEBUG
		printf("attached %p in %d (first: %p)\n", header_ptr, i, GET_FIRST(i));
		mm_check();
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
	void* p;
#ifdef DEBUG
	printf("exit\n");
	mm_check();
	printf("heap_start: %p, mem_heap_hi: %p\n", heap_start, mem_heap_hi());
#endif
	for(p=heap_start; p<=mem_heap_hi(); p+=GET_SIZE(*(size_t*)p)){
		printf("p: %p\n", p);
		mm_free(p+SIZE_T_SIZE);
	}
}
