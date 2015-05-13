#define _GNU_SOURCE
#include <stdio.h>
#include <dlfcn.h>
#include <stdlib.h>

static void *(*mallocp)(size_t);
static void (*freep)(void*);

static void **blocks;
#define BULKSIZE 100
static int blocknum;

static int malloc_cnt=0;
static int invalid_free=0;

int isFirst(void) {
	static int firsttime=1;
	if (firsttime) {
		firsttime = 0;
		return 1;
	} else {
		return 0;
	}
}

void report(void) {
	printf("=============== Test Result =================\n");
	printf("%d unfreed block(s)\n", malloc_cnt);
	printf("%d free(s) with invalid address(maybe double free?)\n", invalid_free);
	if(blocks) freep(blocks);
}

void init(void) {
	char *error;

	malloc_cnt = 0;
	invalid_free = 0;

	if(atexit(report) != 0) {
		printf("Initialization failed\n");
		exit(1);
	}
	if (!mallocp) {
		mallocp = dlsym(RTLD_NEXT, "malloc");
		if ((error = dlerror()) != NULL) {
			fputs(error, stderr);
			exit(1);
		}
	}
	if (!freep) {
		freep = dlsym(RTLD_NEXT, "free");
		if ((error = dlerror()) != NULL) {
			fputs(error, stderr);
			exit(1);
		}
	}

	if(blocks) freep(blocks);
	blocks = mallocp(BULKSIZE);
}

void push(void *ptr) {
	if (blocknum < malloc_cnt + 1) {
		blocknum += BULKSIZE;
		blocks = realloc(blocks, blocknum);
#ifdef DEBUG
		printf("Changed number of blocks: %d\n", blocknum);
#endif
	}

	blocks[malloc_cnt++] = ptr;
}

int pop(void *ptr) {
	int i, j;
	for (i=0; i<malloc_cnt; i++) {
		if (blocks[i] == ptr) {
			/* shift left */
			for (j=i+1; j<malloc_cnt; j++) {
				blocks[j-1] = blocks[j];
			}
			malloc_cnt--;
			return 0;
		}
	}

	/* could not found */
	invalid_free++;
	return 1;
}

void *malloc(size_t size) {
	void *ptr;

	if(isFirst()) init();

	ptr = mallocp(size);
	push(ptr);
#ifdef DEBUG
	printf("malloc(%d) = %p\n", (int)size, ptr);
#endif
	return ptr;
}

void free(void *ptr)
{
	if(isFirst()) init();

#ifdef DEBUG
	printf("free(%p)\n", ptr);
#endif
	if(!pop(ptr)) freep(ptr);
}
