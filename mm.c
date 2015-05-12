/*
 * mm.c - A powerful dynamic allocation gwajae made by Suhyun Lee
 *
 * In this implementation, I used segregated free list to get both
 * efficient memory usage and high throughput.
 * Every block contains header, footer, and space for pointers to
 * other free blocks.
 * Below is list of features supported in and charactoristics of this implementation:
 * - Split
 * - Two way coalescing
 * - Explicit free list
 * - Segregated free lists
 *
 * block structure
 * ==========
 * | header |
 * | next   |
 * | prev   |
 * | ...    |
 * | footer |
 * ==========
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
#define ATTACH(ptr, index) \
	if (GET_FIRST(index) == NULL) { \
		GET_FIRST(index) = (ptr);	\
		NEXT(ptr) = NULL;	\
		PREV(ptr) = NULL; \
	} else { \
		NEXT(ptr) = GET_FIRST(index); \
		PREV(ptr) = NULL; \
		PREV(GET_FIRST(index)) = (ptr); \
		GET_FIRST(i) = (ptr); \
	}

#define DETACH(ptr, index) \
	if (PREV(ptr) == NULL) GET_FIRST(index) = NEXT(ptr);	/* if this is the first free block */ \
	else									NEXT(PREV(ptr)) = NEXT(ptr); \
	if (NEXT(ptr) != NULL) PREV(NEXT(ptr)) = PREV(ptr);
#define GET_INDEX(size, var) \
		for ((var)=0; (var)<SEGLIST_NUM-1; (var)++) \
			if ((0x1<<(var)) < (size) && (0x1<<((var)+1)) >= (size)) break;


static void* heap_start;

//#define DEBUG
//#define CHECK

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
 * mm_check_non_free() detects non-free blocks in the free list
 */
int mm_check_non_free(void) {
	void* p;
	int i, cnt, total;

	printf("[Non-free block detection]\n");
	total = 0;
	for(i=0; i<SEGLIST_NUM; i++) {
		cnt = 0;
		p = GET_FIRST(i);
		while(p != NULL) {
			if (IS_ALLOCATED(GET_HEADER(p))) {
				cnt++;
				total++;
			}
			p = NEXT(p);
		}
		if (cnt!=0)		printf("seglist[%d]: %d\n", i, cnt);
	}
	if(total == 0)	printf("Test passed\n");
	else						printf("Test failed with %d non-free blocks in the seglist\n", total);
	return (total == 0)? 1 : 0;
}

/*
 * mm_check_non_coal() detects non-coalesced free blocks
 */
int mm_check_non_coal(void) {
	void* p = heap_start;
	size_t header;
	void* next;
	int cnt = 0;
	printf("[Non-coalesced free block detection]\n");
	while(p <= mem_heap_hi()) {
		header = GET_HEADER(p);
		next = p + GET_SIZE(header);
		if (next <= mem_heap_hi() &&
				!IS_ALLOCATED(header) && !IS_ALLOCATED(GET_HEADER(next)))
			cnt++;
		p = next;
	}
	if (cnt == 0)	printf("Test passed\n");
	else					printf("Test failed with %d non-coalesced free block(s)\n", cnt);
	return (cnt == 0)? 1 : 0;
}

/*
 * mm_check checks things written below:
 * 1. Detects non-free blocks in free list
 * 2. Detects free blocks that are not coalesced
 */
void mm_check(void) {
	int total = 0;
	printf("========== HEAP CONSISTENCY CHECK ==========\n");
	total += mm_check_non_free();
	total += mm_check_non_coal();
	printf("%d tests passed\n", total);
	printf("\n");
}


void mm_show(void) {
	void** p;
	int i=0;
	printf("========== Current Status ==========\n");
	printf(" memory size: %d bytes\n", mem_heapsize());
	printf(" seglist info: \n");
	for(p=mem_heap_lo(); p<(void**)mem_heap_lo()+ALIGN(SEGLIST_NUM); p++, i++)
		printf("[%d(%d < size <= %d)] start: %p\n", i, 1<<i, 1<<(i+1), *p);
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
#ifdef CHECK
	mm_check();
#endif
#ifdef DEBUG
	mm_show();
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
	void* split = NULL;
	void* next;
	size_t header;
	size_t headersize;
	int diff;
	int i;

	void* first;
	
#ifdef DEBUG
	printf("malloc called with req: %d\n", size);
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
				/* write free pointer */
				if (diff >= MIN_SIZE && split != NULL) {
#ifdef DEBUG
					printf("detach: curr: %p, prev: %p, next: %p\n", p, PREV(p), NEXT(p));
#endif
					DETACH(p, i)
#ifdef DEBUG
					printf("detached %p from index:%d\n", p, i);
#endif

					GET_INDEX(diff, i)
					ATTACH(split, i)
#ifdef DEBUG
					printf("attached %p in index:%d\n", split, i);
#endif
				} else {
					DETACH(p, i)
				}
#ifdef CHECK
				mm_check();
#endif
#ifdef DEBUG
				mm_show();
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

#ifdef CHECK
		mm_check();
#endif
#ifdef DEBUG
		mm_show();
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
		ATTACH(header_ptr, i)
		
#ifdef DEBUG
		printf("attached %p in %d (first: %p)\n", header_ptr, i, GET_FIRST(i));
#endif
#ifdef CHECK
		mm_check();
#endif
#ifdef DEBUG
		mm_show();
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
#ifdef CHECK
	mm_check();
#endif
#ifdef DEBUG
	printf("exit\n");
	printf("heap_start: %p, mem_heap_hi: %p\n", heap_start, mem_heap_hi());
#endif
	for(p=heap_start; p<=mem_heap_hi(); p+=GET_SIZE(*(size_t*)p)){
		mm_free(p+SIZE_T_SIZE);
	}
}
