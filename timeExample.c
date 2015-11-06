#include <stdio.h>
#include <sys/time.h>

int main() {
    struct timeval stop;
    time_t velho = stop.tv_sec;
    while(1) {
	gettimeofday(&stop, NULL);
	if(stop.tv_sec - velho > 3) {
		velho = stop.tv_sec;
		printf("passou 3 segundos \n");	
	}
        //gettimeofday(&stop, NULL);
        //printf("%lu --- %lu\n",stop.tv_sec, stop.tv_usec);
    }   
}
