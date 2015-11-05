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

Size: 1 + 1 + 1 + 1 + 1 + data + 1 + 1 = 7 + data.

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
#include <errno.h>

typedef struct Message {
    unsigned char init;
    unsigned char len;
    unsigned char seq;
    unsigned char dest; // I will send my machine number.
    unsigned char orig;
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
#define NOTDATALENGTH 7

int Seq = 0; // My maximum sequency is 255.
int Token = -1; // -1 If I dont know if I have the token, 0 if I dont have, 1 if I have.
int MyMachine = -1; // The number of my machine(1-4).
int Next,Prev; // Number of the previous / next machine (1-4).
int In,Out; // Number of the In port and the Out port (from my machine).
int SockIn,SockOut; // Socket in = server socket, Socket out = client socket.
struct sockaddr_in SocketS, SocketC;  /* SocketS = server, SocketC = client */
char **Hosts;
struct timeval TBegin,TEnd,RBegin,REnd; // TBegin = Time I got token. TEnd = Time my token will expire.
                                        // RBegin = Time I send my token (for recovery). REnd = time I have to create a new token.

unsigned char calculate_parity(Message m);
Message receive_msg();
void print_message(Message m);

void flush_buf() { // Removes a remaining \n from stdin (in case we do a scanf("%d"))
    char c;
    while((c = getchar()) != '\n');
    return ;
}

char *msg_to_str(Message m) {
    int i,len = (int)m.len;
    printf("Convertendo tamnho = %d\n",len);
    char *aux,*s = malloc(len + 18);
    s = memcpy(s,&m,len+NOTDATALENGTH);
    s[len+NOTDATALENGTH] = '\0';
//     This comments are in case the solution above does not work.
    /*aux = s;
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
    /**s = m.dest;
    s++;
    *s = m.orig;
    s++;
    for(i = 0; i < len; i++) {
        *s = m.data[i];
        s++;
    }
    *s = m.parity;
    s++;
    *s = m.status;
    s++;
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
//    printf("Estou retornando: '%s'\n",aux);
/*    printf("Estou retornando: '");
    for(i=0; i<m.len + 10; i++) {
        printf("%c",s[i]);
    }
    printf("'\n");*/
    return s; // FICAR DE OLHO AQUI!! Pode ser que eu tenha que retornar aux, e nao s.
}

Message create_msg(char *s,int status,int destiny) {
/* Parameter s is only data. Status = 1 -> msg is token. Status = 2 -> msg is monitor. Status = 0 -> msg is data. */
    Message m;
    //&m = malloc(strlen(s) + 18);
    m.init = INIT;
    m.len = strlen(s);
    m.seq = Seq;
    Seq = (Seq++) % 256;
    //memcpy(m.dest,   ,6); // What should I do here?
    //memcpy(m.orig,   ,6); // What should I do here?
    m.dest = destiny;
    m.orig = MyMachine; // This sinalizes me!
    m.data = malloc(strlen(s) + 1);
    m.data = strcpy(m.data, s);
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

Message str_to_msg(char *s,int len) {
    Message m;
    //&m = malloc(strlen(s) + 18); // Im not sure if I need to allocate memory, m is not a pointer, but...
    //memcpy(&m,s,strlen(s)+17);
    memcpy(&m,s,len);
    return m;
}

unsigned char calculate_parity(Message m) {
    int i;
    unsigned char res = 0;
    res = m.len ^ m.seq ^ m.status ^ m.dest ^ m.orig;
    for(i=0; i < (int)m.len; i++) {
        res = res ^ m.data[i];
    }
    return res;
}

int remove_msg() {
/* This function should remove the message I previously sent from the ring. Return 1 on success, 0 on failure. */
    Message m;
    m = receive_msg();
    if(m.orig == MyMachine) {
        puts("Message removed succesfully.");
        return 1;
    }
    return 0;
}

int add_buffer(char **buf, int *len, int first, char *s,int dest,int *destVec) {
/* Parameters: Buffer, how many strings I have on the buffer and the string I will add in the buffer.
   I should add the string s to my buffer, so I can send it when I get the token.
   Returns 1 on success, 0 on failure (unable to allocate memory).
   PS: I dont have to use destLen neither destFirst, its the same position as buffer.*/
    printf("Adding '%s' to my buf. Len = %d,first=%d,dest=%d.\n",s,*len,first,dest);
    if(*len == BUF_MAX) // Buffer is full. I could, insted of return 0, realloc my buffer and make it a smart buffer. Maybe later.
        return 0;
    int pos = (*len + first) % BUF_MAX; // Circular.
    if((buf[pos] = malloc(sizeof(char) * (strlen(s) + 1))) == NULL) {
        puts("(add_buffer) Unable to allocate memory.");
        return 0;
    }
    strcpy(buf[pos],s);
    destVec[pos] = dest; // I added the string, I have to know its destiny.
    (*len)++;
    puts("Added succesfully.");
    return 1;
}

Message receive_msg() {
/* This function should be triggered whenever there is a new message in the socket.
   It will read a new message and return it. */
    Message m;
    char *s = malloc(1024);
    s[0] = '\0';
    int x = sizeof(SocketS),len = 0;
    while(s[0] != INIT) {
        len = recvfrom(SockIn, s, 1024, 0, (struct sockaddr *) &SocketS, &x);
    }
    printf("Received %d bytes",len);
    puts("");
//    printf("Received :'%s'\n",s);
    int i;
/*    printf("Received : ");
    for(i=0; i<len; i++) {
        printf("%c",s[i]);
    }
    printf("\n");*/
    m = str_to_msg(s,len);
    print_message(m);
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
/* I will create a timeout of x miliseconds, depending on the parameter. If type = 1, its a TOKEN_TIMEOUT.
If type = something other than 1, its a RECOVERY_TIMEOUT (I just sent a token). */
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
    puts("Timeout set.");
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

void print_message(Message m) {
    printf("Msg: Init = %d, Len = %d, Seq = %d, Dest = %d, Orig = %d, Data = %s, Parity = %d, Status = %c\n",
        (int)m.init,(int)m.len,(int)m.seq,(int)m.dest,(int)m.orig,m.data,(int)m.parity,m.status);
}

int rem_buffer(char **buf, int *len, int *first,int *destVec) {
/* This function will remove the first element from the buffer and update its length and first position. */
    free(buf[*first]); // Erasing data.
    destVec[*first] = -1;
    (*len)++;
    ((*first)++) % BUF_MAX; // If first is 1024, it will become 0 again.
    return 1;
}

int send_msg(Message m) {
/* This function should send the string s to my neighbor and return 1 on success and 0 on failure. */
    char *s = malloc(1024);
    print_message(m);
    s = msg_to_str(m);
    //printf("Sending :'%s'\n",s);
    if(sendto(SockOut, s, m.len + NOTDATALENGTH, 0, (struct sockaddr *) &SocketC, sizeof(SocketC)+1) == -1)
        return 0;
    return 1;
}

int send_token() {
/* Send the token to the next machine. */
    puts("Sending token.");
    Message m;
    m = create_msg("\0",1,Next); // Not sure if that \0 is needed.
    send_msg(m);
    return 1;
}

int send_monitor() {
/* Send the token to the next machine. */
    puts("Sending monitor.");
    Message m;
    m = create_msg("\0",2,Next); // Not sure if that \0 is needed.
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

    if (bind(SockIn, (struct sockaddr *) &SocketS,sizeof(SocketS)) < 0){
        puts("Could not bind.");
        exit(1);
    }
}

void create_client(struct hostent *hp) {
    char *host = Hosts[MyMachine % 4];

    memcpy((char*)&SocketC.sin_addr, (char*)hp->h_addr, hp->h_length);
    SocketC.sin_family = hp->h_addrtype;

    SocketC.sin_port = htons(Out);

    if((SockOut = socket(hp->h_addrtype, SOCK_DGRAM, 0)) < 0) {
        puts("Could not open socket.");
        exit(1);
    }
    puts("Client created succesfully! (I guess...)");
}
/*
This is a test to make sure that str to msg and msg to str are working. And they seem to, but I can not print str succesfully.
int main() {
    char *test = malloc(1024);
    scanf("%1024s",test);
    puts(test);
    Message m,m2;
    m = create_msg(test,0,3);
    print_message(m);
    test = msg_to_str(m);
    m2 = str_to_msg(test);
    print_message(m2);
    puts("Done");
    return 1;
}
*/
int main(int argc, char* argv[]) {
    int timeout_msecs = 200,expired,dest,*destVec;
    int i=0,bufLen = 0,bufFirst = 0,destLen = 0,destFirst = 0,type;
    char **buf;
    char *s,*localhost;
    struct hostent *hp,*hp2;
    struct pollfd fds[2],fdAux = { STDIN_FILENO, POLLIN|POLLPRI };
    Message m;

    destVec = malloc(sizeof(int) * BUF_MAX);
    buf = malloc(sizeof(char*) * BUF_MAX);
    localhost = malloc(MAX_HOSTNAME);
    s = malloc(1026);
    s[0] = '\0';
    fds[0] = fdAux;
    fds[1].fd = In;
    Hosts = malloc(sizeof(char*) * 4);
    Hosts[0] = "bowmore"; // 1
    Hosts[1] = "orval"; // 2
    Hosts[2] = "achel"; // 3
    Hosts[3] = "latrappe"; // 4
    gethostname(localhost, MAX_HOSTNAME);

    if(argc != 2) {
        puts("Correct way to opearate: <MyMachine number>");
        return -1;
    }

/*  MyMachine     Port(in) Port(out)
    1           PORT     PORT+1
    2           PORT+1   PORT+2
    3           PORT+2   PORT+3
    4           PORT+3   PORT       */
    MyMachine = argv[1][0] - 48;
    if(MyMachine == 1) {
        Token = 1;
        set_timeout(1); // 1 means token_timeout.
        Next = MyMachine+1;
        Prev = MyMachine+3;
        In = PORT + MyMachine - 1;
        Out = PORT + MyMachine;
    } else if(MyMachine == 4) {
        Next = MyMachine-3;
        Prev = MyMachine-1;
        Token = 0;
        In = PORT + MyMachine - 1;
        Out = PORT;
    } else if(MyMachine == 2 || MyMachine == 3) {
        Next = MyMachine+1;
        Prev = MyMachine-1;
        Token = 0;
        In = PORT + MyMachine - 1;
        Out = PORT + MyMachine;
    } else {
        puts("Please choose a number between 1 and 4.");
        return -1;
    }

    printf("MyMachine = %d, Localhost = %s, Port In = %d, Port Out = %d, Next = %d, Prev = %d\n",MyMachine,localhost,In,Out,Next,Prev);

    puts(msg_to_str(str_to_msg("Testeeeeee",10)));

    if((hp = gethostbyname(localhost)) == NULL) {
        puts("Couldn't get my own IP.");
        return -1;
    }

    create_server(hp); // I do not need to use this parameter, its just to remember that I will have to use it in this function.

    puts("When all machines have set up the server, type any number to start connecting clients.");
    scanf("%d",&i);
    flush_buf();

    printf("The next machine name is: %s\n",Hosts[Next-1]);

    if((hp2 = gethostbyname(Hosts[Next-1])) == NULL) {
        puts("Couldn't get next machine IP.");
        return -1;
    }

    create_client(hp2);

    if(MyMachine == 1) {
        send_msg(create_msg("Teste",0,Next));
    } else if(MyMachine == 2) {
        int asdf = 0;
        int randomname = sizeof(SocketS);
        asdf = recvfrom(SockIn, s, 1024, 0, (struct sockaddr *) &SocketS, &randomname);
        s[asdf] = '\0';
        //if(asdf > 0) {
        //    printf("Received : ");
        //    puts(s);
        //}
        printf("Received %d bytes.\n",asdf);
        print_message(str_to_msg(s,asdf));
    }

    while(1) {
        while(Token == 1 && bufLen > 0) {
            puts("I have the token!! Oh yeah!");
            Message m = create_msg(buf[bufFirst],0,destVec[bufFirst]);
            while(!send_msg(m)) { // Send a message with my string s.
                expired = timedout();
                if(expired == 1) {
                    send_token();
                    set_timeout(2);
                }
            }
            while(!remove_msg()) {
                expired = timedout();
                if(expired == 2) { // We lost our token.
                    Token = 1;
                    send_monitor();
                }
            }
            /*if(poll(&(fds[1]),1,timeout_msecs)) {
                remove_msg();
            }*/
            rem_buffer(buf,&bufLen,&bufFirst,destVec);
            if(timedout() == 1) { // Token time out.
                send_token();
                set_timeout(2); // 2 means recovery_timeout.
            }
        }
        if(poll(fds, 2, timeout_msecs) ) { // There is something to be read. Message or stdin.
            if(fds[0].revents & POLLIN|POLLPRI) { // Got something in STDIN.
                fgets(s,1025,stdin); // Is 1023 enough? See * (down there).
                int dest = s[0] - 48;
                if(dest < -1 || dest > 5) {
                    puts("Format not known.");
                } else {
                    if(dest == 0) {
                        puts("Throwing token away...");
                        Token = 0;
                    }
                    s += 2; // My data shall not contain the two first characters - machine number and space. ("3 ")
                    if(Token == 1) {
                        puts("I will be sending a message.");
                        Message m = create_msg(s,0,dest);
                        while(!send_msg(m)) { // Send a message with my string s.
                            puts("Problem sending message.");
                            printf("\tError was: %s\n",strerror(errno));
                            /*expired = timedout();
                            if(expired == 1) {
                                send_token();
                                set_timeout(2);
                            }*/
                            scanf("%d",&expired);
                        }
                        while(!remove_msg()) {
                            puts("Problem removing message.");
                            expired = timedout();
                            if(expired == 2) { // We lost our token.
                                Token = 1;
                                send_monitor();
                            }
                        }
                    } else {
                        add_buffer(buf,&bufLen,bufFirst,s,dest,destVec);
                    }
                    s -= 2;
                }
            } else if(fds[1].revents & POLLIN) { // Got a message! (Remember to check this & (AND).
                m = receive_msg(); // What structure is my message? Could it be only a string?
                type = typeof_msg(m);
                if(type == 1) { // Got a token
                    Token = 1;
                    set_timeout(1); // 1 means token_timeout.
                } else if(type == 2) { // Got a monitor - someone created a new token!
                    Token = 2;
                    send_msg(m);
                } else {
                    if(m.dest == MyMachine || m.dest == 5) { // Its for me!
                        print_message(m);
                    }
                    send_msg(m);
                }
            } else {
                // Got nothing. Something wrong occured. I should have received a token or a monitor at least.
                Token = 1;
                send_token();
                set_timeout(2);
                puts("Token sent. Setting timeout.");
            }
            if(expired = timedout() || bufLen == 0) {
                if(expired == 1 || bufLen == 0) { // My token timedout or my buffer is empty.
                    send_token();
                    set_timeout(2); // 2 means recovery_timeout.
                } else if(expired == 2) { // Recovery timeout expired.
                    Token = 1;
                    send_monitor();
                }
            }
        }
    }
}

/*
About typing format: Your message should be something like:
1 All the things I want to say to machine number 1.
It will be read in this way: 1 digit (In that example, 1), which is the machine number, a space and then text.
*/
