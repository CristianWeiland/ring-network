#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <poll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/types.h>
#include <string.h>
#include <sys/time.h>
#include <errno.h>

int In,Out; // Number of the In port and the Out port (from my machine).
int SockIn,SockOut; // Socket in = server socket, Socket out = client socket.
struct sockaddr_in SocketS, SocketC;  /* SocketS = server, SocketC = client */

void flush_buf() { // Removes a remaining \n from stdin (in case we do a scanf("%d"))
    char c;
    while((c = getchar()) != '\n');
    return ;
}


void create_server(struct hostent *hp) {
/* Inicialize server */
    SocketS.sin_port = htons(In);
    memcpy((char*)&SocketS.sin_addr, (char*)hp->h_addr, hp->h_length);
    SocketS.sin_family = hp->h_addrtype;
    
    if ((SockIn = socket(hp->h_addrtype, SOCK_DGRAM, 0)) < 0){
        puts("Could not open socket.");
        exit(1);
    }

    if (bind(SockIn, (struct sockaddr *) &SocketS,sizeof(SocketS)) < 0){
        puts("Could not bind.");
        exit(1);
    }
}   

void create_client(struct hostent *hp) {
    char *host = "orval";

    memcpy((char*)&SocketC.sin_addr, (char*)hp->h_addr, hp->h_length);
    SocketC.sin_family = hp->h_addrtype;

    SocketC.sin_port = htons(Out);

    if((SockOut = socket(hp->h_addrtype, SOCK_DGRAM, 0)) < 0) {
        puts("Could not open socket.");
        exit(1);
    }
    puts("Client created succesfully! (I guess...)");
}

int main() {
    int timeout_msecs = 200,expired,dest,*destVec;
    int i=0,bufLen = 0,bufFirst = 0,destLen = 0,destFirst = 0,type;
    char **buf;
    char *s,*localhost;
    struct hostent *hp,*hp2;

    In = 33334;
    Out = 33333;
    printf("In = %d",In);

    localhost = malloc(30);
    s = malloc(1026);
    s[0] = '\0';
    gethostname(localhost, 29);

    if((hp = gethostbyname(localhost)) == NULL) {
        puts("Couldn't get my own IP.");
        return -1;
    }

    create_server(hp); // I do not need to use this parameter, its just to remember that I will have to use it in this function.

    puts("When all machines have set up the server, type any number to start connecting clients.");
    scanf("%d",&i);
    flush_buf();

    if((hp2 = gethostbyname("bowmore")) == NULL) {
        puts("Couldn't get next machine IP.");
        return -1;
    }

    create_client(hp2);

    int x = sizeof(SocketS),len = 0;
    while(1) {
        len = recvfrom(SockIn, s, 1024, 0, (struct sockaddr *) &SocketS, &x);
        //puts(s);
        s[len] = '\0';
        if(len > 0) {
            printf("Received : ");
            puts(s);
        }
        len = 0;
    }
}
