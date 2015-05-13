#include <stdio.h>
#include <stdlib.h>

int main(void) {
	int *p1, *p2;

	p1 = malloc(10);
	p2 = malloc(20);
	p1[0] = 10;
	p2[0] = 20;

	printf("p1[0]: %d\n", p1[0]);
	printf("p2[0]: %d\n", p2[0]);

	free(p1);
	free(p2);
	return 0;
}
