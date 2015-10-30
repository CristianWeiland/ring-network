#include <stdio.h>
#include <sys/time.h>

int main() {
	struct timeval stop, start;
	gettimeofday(&start, NULL);

	gettimeofday(&stop, NULL);
	printf("took %lu === %lu === %lu\n", stop.tv_usec - start.tv_usec,start.tv_usec,start.tv_sec);
}