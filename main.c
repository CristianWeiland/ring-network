// http://www.inf.ufpr.br/elias/redes/servudp.c.txt
// http://www.inf.ufpr.br/elias/redes/cliudp.c.txt
// <3 Elias


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <poll.h>

#define THIS 0 // It sinalizes which machine Im on.
#define TOKEN_TIMEOUT 100 // How much time will I stay with the token?
#define RECOVERY_TIMEOUT 1000 // How much time will I wait before I create a new token? I have to calculate it!

int Token = -1;
int Next,Prev;

int send_msg(char *s) {
/* This function should send the string s to my neighbor and return 1 on success and 0 on failure. */
}

int remove_msg(char *s) {
/* This function should remove the message I previously sent from the ring. Return 1 on success, 0 on failure. */
}

int add_buffer(char **buf,int *len,char *s) {
/* Parameters: Buffer, how many strings I have on the buffer and the string I will add in the buffer.
   I should add the string s to my buffer, so I can send it when I get the token.
   Returns 1 on success, 0 on failure (unable to allocate memory). */
    if(buf[*len] = malloc(sizeof(char) * (strlen(s) + 1)) == NULL) {
        puts("(add_buffer) Unable to allocate memory.");
        return 0;
    }
    strcpy(buf[*len],s);
    (*len)++;
    return 1;
}

??? receive_msg() {
/* This function should be triggered whenever there is a new message in the socket.
   It will read a new message and return it. */
}

char typeof_msg(??? m) {
/* It should return if my message is a token or data */
}

void set_timeout(int timeout) {
/* I will create a timeout of 'timeout' miliseconds. */
}

void print_message() {

}

int timedout() {
/* Check if a timeout occured. */
}

int buffer_is_empty() {
/* It will check if buf len is 0. Maybe we dont need this function. */
}

int send_token() {
/* Send the token to the next machine. */
}

int main(int argc, char* arv[]) {
    struct pollfd fds[2];
    fds[0] = { STDIN_FILENO, POLLIN|POLLPRI };
    fds[1].fd = Socket;
    int timeout_msecs = 500;
    int i=0;
    char *s;
    s = malloc(1024);
    s[0] = '\0';
/*
    int sockdescr;
    int numbytesrecv;
    struct sockaddr_in sa;
    struct hostent *hp;
    char buf[BUFSIZ+1];
    char *host;
    char *dados;
*/
    if(argc != 2) {
        puts("Correct way to opearate: <Machine number> <port>");
        return -1;
    }

    Token = args[1] - 33; // This probably does not work. Check it later please.
    if(Token == 1) {
        create_token();
        Next = port+1;
        Prev = port+3;
    } else if(Token == 4) {
        Next = port-3;
        Prev = port-1;
    }

    create_server(port);

    puts("When all machines have set up the server, type 1 to start connecting clients.");
    scanf("%d",&i);
    if(i != 1) {
        puts("Some problem occured when setting up clients. Exiting...");
        return -1;
    }

    create_client(port);

    while(1) {
        if( poll(fds, 2, timeout_msecs) ) { // There is something to be read. Message or stdin.
            if(fds[0].revents & POLLIN|POLLPRI) { // Got something in STDIN.
                fgets(s,1023,stdin); // Is 1023 enough?
                if(Token == THIS) {
                    while(!send_msg(s)); // Send a message with my string s.
                    while(!remove_msg(s));
                } else {
                    add_buffer(s);
                }
                                    // I have to search this & with datagram socket possibly. Is it pollin?
            } else if(fds[1].revents & POLLIN) { // Got a message!
                receive_msg(); // What structure is my message? Could it be only a string?
                if(typeof_msg() == 't') { // t is for token.
                    Token = THIS;
                    set_timeout(TOKEN_TIMEOUT)
                } else {
                    print_message();
                }
            } else {
                // Got nothing. Do nothing.
            }
            if(timedout() || buffer_is_empty()) {
                send_token();
                set_timeout(RECOVERY_TIMEOUT);
            }
        }
            /*
            scanf("%s",s);
            printf("Li: %s\n",s);
        } else { // There is nothing in STDIN.
            puts("Nada nos parametros.");
        }
        i++;*/
    }
}
