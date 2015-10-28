#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <poll.h>

int main() {
    struct pollfd fds = { STDIN_FILENO, POLLIN|POLLPRI };
    int timeout_msecs = 500;
    int i=0;
    char *s;
    s = malloc(1024);
    s[0] = '\0';

    while(i < 20) {
//      printf("Tentando receber algo '%s'...",s);
        if( poll(&fds, 1, timeout_msecs) ) {
            scanf("%s",s);
            printf("Li: %s\n",s);
        } else {
            puts("Nada nos parametros.");
        }
        i++;
    }
}
