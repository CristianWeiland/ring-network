/*
http://www.inf.ufpr.br/elias/redes/servudp.c.txt
http://www.inf.ufpr.br/elias/redes/cliudp.c.txt
*/
/*
Encapsulation:
Start Delimiter - 8 bits
Length
Sequency
Dest Add
Orig Add
Data
Vertical Parity - 8 bits
Message Status with Token/Monitor - 8 bits
Status: A C T M A C 0 0 no qual: A = Address recognized (got message), C = Message Copied (message had no error), T = Token (is token?), M = Monitor (is monitor?)

Machine number 1 = bowmore
Machine number 2 = orval
Machine number 3 = achel
Machine number 4 = latrappe
*/

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

typedef struct Message {
    unsigned char init;
    unsigned char len;
    unsigned char seq;
    unsigned char dest[6];
    unsigned char orig[6];
    unsigned char *data;
    unsigned char parity;
    unsigned char status;
} Message;

#define TOKEN_BIT 32
#define MONITOR_BIT 16
#define INIT 126
#define MAX_HOSTNAME 64
#define TOKEN_TIMEOUT 100 // How much time will I stay with the token?
#define RECOVERY_TIMEOUT 1000 // How much time will I wait before I create a new token? I have to calculate it!
#define PORT 47237 // The base port, used to get all 4 ports (which will be 47237,47238,47239,47240).
#define BUF_MAX 1024

int Seq = 0; // My maximum sequency is 255.
int Token = -1; // -1 If I dont know if I have the token, 0 if I dont have, 1 if I have.
int Machine = -1; // The number of my machine(1-4).
int Next,Prev; // Number of the previous / next machine (1-4).
int In,Out; // Number of the In port and the Out port (from my machine).
int SockIn,SockOut; // Socket in = server socket, Socket out = client socket.
struct sockaddr_in SocketS, SocketC;  /* SocketS = server, SocketC = client */
char **Hosts;
struct timeval TBegin,TEnd,RBegin,REnd; // TBegin = Time I got token. TEnd = Time my token will expire.
                                        // RBegin = Time I send my token (for recovery). REnd = time I have to create a new token.



char *msg_to_str(Message m) {
    int i,len = strlen(m->data);
    char *aux,*s = malloc(len + 18);
    memcpy(s,&m,len+17);
    s[len+17] = '\0';
    /* // This comments are in case the solution above does not work.
    aux = s;
    *s = m.init;
    s++;
    *s = m.len;
    s++;
    *s = m.seq;
    s++;
    for(i = 0; i < 6; i++) {
        *s = m.dest[i];
        s++;
    }
    for(i = 0; i < 6; i++) {
        *s = m.orig[i];
        s++;
    }
    for(i = 0; i < len; i++) {
        *s = m.data[i];
        s++;
    }
    *s = parity;
    s++;
    *s = status;
    s++
    *s = '\0';
    /*
    s[0] = m.init;
    s[1] = m.len;
    s[2] = m.seq;
    for(i = 0; i < 6; i++) {
        s[i+3] = m.dest[i];
        s[i+9] = m.orig[i];
    }
    for(i = 0; i < len; i++) {
        s[i+15] = m.data[i];
    }
    s[len+15] = parity;
    s[len+16] = status;
    */
}

Message create_msg(char *s,int status) {
/* Parameter s is only data. Status = 1 -> msg is token. Status = 2 -> msg is monitor. Status = 0 -> msg is data. */
    Message m = malloc(strlen(s) + 18);
    m.init = INIT;
    m.len = strlen(s);
    m.seq = Seq;
    Seq = (Seq++) % 256;
    memcpy(m.dest,   ,6); // What should I do here?
    memcpy(m.orig,   ,6); // What should I do here?
    m.data = strcpy(m.data, s, strlen(s));
    if(status == 1) {
        m.status = TOKEN_BIT;
    } else if(status == 2) {
        m.status = MONITOR_BIT;
    } else {
        m.status = 0;
    }
    m.parity = calculate_parity(m);
    return m;
}

Message str_to_msg(char *s) {
    Message m = malloc(strlen(s) + 18);
    memcpy(m,s,strlen(s)+17);
    return m;
}

unsigned char calculate_parity(Message m) {
    int i;
    unsigned char res = 0;
    res = m.len ^ m.seq ^ m.status;
    for(i=0; i<6; i++) {
        res = res ^ m.dest[i] ^ m.orig[i];
    }
    for(i=0; i < (int)m.len; i++) {
        res = res ^ m->data[i];
    }
    return res;
}

int send_msg(Message m) {
/* This function should send the string s to my neighbor and return 1 on success and 0 on failure. */
    char *s;
    s = msg_to_str(m);
    sendto(Out, s, strlen(s), 0, (struct sockaddr *) &SocketC, sizeof(SocketC));
    return 1;
}

int remove_msg(char *s) {
/* This function should remove the message I previously sent from the ring. Return 1 on success, 0 on failure. */
    Message m;
    m = receive_msg();
    if(strcmp(m.orig,EU) == 0) {
        return 1;
    }
    return 0;
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

Message receive_msg() {
/* This function should be triggered whenever there is a new message in the socket.
   It will read a new message and return it. */
    Message m;
    char *s = malloc(1024);
    s[0] = '\0';
    while(s[0] != INIT) {
        recvfrom(In, s, 1024, 0, (struct sockaddr *) &SocketS, &(sizeof(SocketS)));
    }
    m = str_to_msg(s)
    return m;
}

int typeof_msg(Message m) {
/* It should return if my message is a token (1), monitor(2) or data(0) */
    if(m.status & TOKEN_BIT) {
        return 1;
    } else if(m.status & MONITOR_BIT) {
        return 2;
    } else {
        return 0;
    }
}

void set_timeout(int type) {
/* I will create a timeout of 'timeout' miliseconds. */
    if(type == 1) { // Got the token.
        //gettimeofday(&GotToken, NULL);
        gettimeofday(&TEnd, NULL); // Got time right now. I have to add TOKEN_TIMEOUT.
        TEnd.tv_usec += TOKEN_TIMEOUT;
        /*if(TEnd.tv_usec > 1000 - TOKEN_TIMEOUT) { // I can simply add to usecs, it will overflow.
            TEnd.tv_sec++;
            int sub = 1000 - TEnd.tv_usec;
            TEnd.tv_usec = TOKEN_TIMEOUT - sub;
        } else {
            TEnd.tv_usec += TOKEN_TIMEOUT;
        }*/

    } else { // Token sent, recovery timeout.
        //gettimeofday(&RBegin, NULL);
        gettimeofday(&REnd, NULL); // Got time right now. I have to add TOKEN_TIMEOUT.
        REnd.tv_usec += RECOVERY_TIMEOUT; // Since I am using exactly 1 sec timeout, I can do it this way.
    }
}

int timedout() {
/* Check if a timeout occured. If my Token timedout, return 1. */
    struct timeval now;
    gettimeofday(&now, NULL);
    if(now.tv_usec > TEnd.tv_usec && TEnd.tv_usec != 0) {
        TEnd.tv_usec = 0; // Reset my timer.
        return 1;
    } else if(now.tv_usec > REnd.tv_usec && REnd.tv_usec != 0) {
        REnd.tv_usec = 0;
        return 2;
    } else {
        return 0;
    }
}

/*
    unsigned char init;
    unsigned char len;
    unsigned char seq;
    unsigned char dest[6];
    unsigned char orig[6];
    unsigned char *data;
    unsigned char parity;
    unsigned char status;
*/
void print_message(Message m) {
/*
    char *aux1,*aux2;
    aux1 = malloc(7);
    aux2 = malloc(7);
    aux1 = strcpy(aux1,m.dest);
    aux2 = strcpy(aux2,m.orig);
*/;

    printf("Msg: Init = %c, Len = %c, Seq = %c, Dest = %s, Orig = %s, Data = %s, Parity = %c, Status = %c",
        m.init,m.len,m.seq,m.dest,m.orig,m.data,m.parity,m.status);
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
    Message m;
    m = create_msg("\0",1); // Not sure if that \0 is needed.
    send_msg(m);
    return 1;
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

    if (bind(SockIn, (struct sockaddr *) &sa,sizeof(sa)) < 0){
        puts("Could not bind.");
        exit(1);
    }
}

void create_client(struct hostent *hp) {
    char *host = Hosts[Machine % 4];

    if((hp = gethostbyname(host)) == NULL){
        puts("Could not find server IP.");
        exit(1);
    }

    memcpy((char*)&SocketC.sin_addr, (char*)hp->h_addr, hp->h_length);
    SocketC.sin_family = hp->h_addrtype;

    SocketC.sin_port = htons(Out);

    if((SockOut = socket(hp->h_addrtype, SOCK_DGRAM, 0)) < 0) {
        puts("Could not open socket.");
        exit(1);
    }
}

int main(int argc, char* arv[]) {
    int timeout_msecs = 50,expired;
    int i=0,bufLen = 0,bufFirst = 0,type;
    char **buf;
    char *s,*hostname;
    struct hostent *hp,*hp2;
    struct pollfd fds[2];
    Message m;

    buf = malloc(sizeof(char*) * BUF_MAX);
    hostname = malloc(MAX_HOSTNAME);
    s = malloc(1024);
    s[0] = '\0';
    fds[0] = { STDIN_FILENO, POLLIN|POLLPRI };
    fds[1].fd = Socket;
    Hosts = malloc(sizeof(char*) * 4);
    Hosts[0] = "bowmore"; // 1
    Hosts[1] = "orval"; // 2
    Hosts[2] = "achel"; // 3
    Hosts[3] = "latrappe"; // 4

    get_hostname(localhost, hostname);

    if ((hp = gethostbyname( localhost)) == NULL){
        puts("Couldn't get my own IP.");
        return -1;
    }

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

    create_server(hp); // I do not need to use this parameter, its just to remember that I will have to use it in this function.

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
                m = receive_msg(); // What structure is my message? Could it be only a string?
                type = typeof_msg(m);
                if(type == 1) { // Got a token
                    Token = 1;
                    set_timeout(TOKEN_TIMEOUT);
                } else if(type == 2) { // Got a monitor - someone created a new token!
                    Token = 0;
                    send_msg(m);
                } else {
                    print_message(m);
                    send_msg(m);
                }
            } else {
                // Got nothing. Do nothing.
            }
            if(expired = timedout() || bufLen == 0) {
                if(expired == 1 || bufLen == 0) { // My token timedout or my buffer is empty.
                    send_token();
                    set_timeout(RECOVERY_TIMEOUT);
                } else if(expired == 2) { // Recovery timeout expired.
                    Token = 1;
                    send_monitor();
                }
            }
        }
    }
}
