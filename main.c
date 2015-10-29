// http://www.inf.ufpr.br/elias/redes/servudp.c.txt
// http://www.inf.ufpr.br/elias/redes/cliudp.c.txt
// <3 Elias


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <poll.h>

#define TOKEN_TIMEOUT 100 // How much time will I stay with the token?
#define RECOVERY_TIMEOUT 1000 // How much time will I wait before I create a new token? I have to calculate it!
#define PORT 47237 // The base port, used to get all 4 ports (which will be 47237,47238,47239,47240).
#define BUF_MAX 1024

int Token = -1; // -1 If I dont know if I have the token, 0 if I dont have, 1 if I have.
int Machine = -1; // The number of my machine(1-4).
int Next,Prev; // Number of the previous / next machine (1-4).
int In,Out; // Number of the In port and the Out port (from my machine).
int SockIn,SockOut; // Socket in = server socket, Socket out = client socket.

int send_msg(char *s) {
/* This function should send the string s to my neighbor and return 1 on success and 0 on failure. */
}

int remove_msg(char *s) {
/* This function should remove the message I previously sent from the ring. Return 1 on success, 0 on failure. */
}

int add_buffer(char **buf, int *len, int first, char *s) {
/* Parameters: Buffer, how many strings I have on the buffer and the string I will add in the buffer.
   I should add the string s to my buffer, so I can send it when I get the token.
   Returns 1 on success, 0 on failure (unable to allocate memory). */
    if(*len == BUF_MAX) // Buffer is full. I could, insted of return 0, realloc my buffer and make it a smart buffer. Maybe later.
        return 0;
    int pos = (*len + first) % 1024; // Circular line. (Line eh a traducao certa de fila?)
    if(buf[pos] = malloc(sizeof(char) * (strlen(s) + 1)) == NULL) {
        puts("(add_buffer) Unable to allocate memory.");
        return 0;
    }
    strcpy(buf[pos],s);
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

int rem_buffer(char **buf, int *len, int *first) {
/* This function will remove the first element from the buffer and update its length and first position. */
    free(buf[first]); // Erasing data.
    (*len)++;
    ((*first)++) % BUF_MAX; // If first is 1024, it will become 0 again.
    return 1;
}

int send_token() {
/* Send the token to the next machine. */
}

int main(int argc, char* arv[]) {
    struct pollfd fds[2];
    fds[0] = { STDIN_FILENO, POLLIN|POLLPRI };
    fds[1].fd = Socket;
    int timeout_msecs = 500;
    int i=0,bufLen = 0,bufFirst = 0;
    char **buf;
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

    if(argc != 1) {
        puts("Correct way to opearate: <Machine number>");
        return -1;
    }

/*
    Machine     Port(in) Port(out)
    1           PORT     PORT+1
    2           PORT+1   PORT+2
    3           PORT+2   PORT+3
    4           PORT+3   PORT
*/

    Machine = args[1][0] - 48;
    if(Machine == 1) {
        create_token();
        Token = 1;
        Next = port+1;
        Prev = port+3;
        In = PORT + Machine - 1;
        Out = PORT + Machine;
    } else if(Token == 4) {
        Next = port-3;
        Prev = port-1;
        Token = 0;
        In = PORT + Machine - 1;
        Out = PORT;
    } else if(Token == 2 || Token == 3) {
        Next = port+1;
        Prev = port-1;
        Token = 0;
        In = PORT + Machine - 1;
        Out = PORT + Machine;
    } else {
        puts("Please choose a number between 1 and 4.");
        return -1;
    }

    create_server(In); // I do not need to use this parameter, its just to remember that I will have to use it in this function.

    puts("When all machines have set up the server, type any number to start connecting clients.");
    scanf("%d",&i);

    create_client(Out);

    while(1) {
        while(Token && bufLen > 0) {
            while(!send_msg(buf[0]));
            while(!remove_msg(buf[0]);
            remove_from_buffer(buf,&bufLen,&bufFirst);
            if(timedout() == 1) { // Token time out.
                send_token();
                set_timeout(RECOVERY_TIMEOUT);
            }
        }
        if( poll(fds, 2, timeout_msecs) ) { // There is something to be read. Message or stdin.
            if(fds[0].revents & POLLIN|POLLPRI) { // Got something in STDIN.
                fgets(s,1023,stdin); // Is 1023 enough?
                if(Token) {
                    while(!send_msg(s)); // Send a message with my string s.
                    while(!remove_msg(s));
                } else {
                    add_buffer(buf,&bufLen,first,s);
                }
            } else if(fds[1].revents & POLLIN) { // Got a message! (Remember to check this & (AND).
                receive_msg(); // What structure is my message? Could it be only a string?
                if(typeof_msg() == 't') { // t is for token.
                    Token = 1;
                    set_timeout(TOKEN_TIMEOUT);
                } else {
                    print_message();
                    send_msg();
                }
            } else {
                // Got nothing. Do nothing.
            }
            if(timedout() || bufLen == 0) {
                send_token();
                set_timeout(RECOVERY_TIMEOUT);
            }
        }
    }
}
