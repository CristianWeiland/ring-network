/*
Encapsulation:

+-----------------+--------+----------+---------+--------+------+--------+--------+
| Start Delimiter | Length | Sequency | Destiny | Origin | Data | Parity | Status |
+-----------------+--------+----------+---------+--------+------+--------+--------+
Size:      8 bits   8 bits     8 bits    8 bits   8 bits    ???   8 bits   8 bits

Start Delimiter - 01111110 - 126.
Length - Data length
Sequency - Message sequency
Destiny - Destiny address (A number from 1-5, 1-4 being machine number (see below) and 5 being everybody.)
Origin - Origin address (A number from 1-4 representing the machine number).
Data - Data, with unknown length.
Parity - Vertical parity.
Status - A C T M A C 0 0, being: A = Address recognized (got message), C = Message Copied (message had no error), T = Token (is token?), M = Monitor (is monitor?)

Total Size: 1 + 1 + 1 + 1 + 1 + data + 1 + 1 = 7 + data.

+----------------+--------------+---------+----------+
| Machine Number | Machine Name | Port In | Port Out |
+----------------+------------------------+----------+
|              1 |      Bowmore |   47237 |    47238 |
|              2 |        Orval |   47238 |    47239 |
|              3 |        Achel |   47239 |    47240 |
|              4 |     Latrappe |   47240 |    47237 |
+----------------+--------------+---------+----------+

To send a message you should use this format:
<Destiny machine number (1-4)> <data I want to send>
Ex: "2 You shall not pass!" would send to machine 2 (orval) "You shall not pass".

To send a message to everbody the destiny must be 5.
Ex: "5 Everybody copy?"

To simulate a fail send_token(), you should use this message:
"0"
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
#define TOKEN_TIMEOUT 1000 // How much time will I stay with the token? (in seconds)
#define RECOVERY_TIMEOUT 3000 // How much time will I wait before I create a new token? I have to calculate it! (in seconds)
#define PORT 47237 // The base port, used to get all 4 ports (which will be 47237,47238,47239,47240).
#define BUF_MAX 1024
#define NOTDATALENGTH 7
#define DATA_TYPE 0
#define TOKEN_TYPE 1
#define MONITOR_TYPE 2

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

char* remove_enter(char *s) {
    int len = strlen(s);
    s[len-1] = '\0';
    return s;
}

char* msg_to_str(Message m) {
    int i,len = (int)m.len;
    char *aux,*s = malloc(len + NOTDATALENGTH + 1);
    aux = s;
    aux = memcpy(aux,&m,5); // Copy init, len, seq, dest, orig
    aux += 5;
    memcpy(aux,m.data,len); // Copy m.data
    aux += len;
    memcpy(aux,&m.parity,2); // Copy parity and status
    aux += 2;
    *aux = '\0';
    return s;
}

Message create_msg(char *s,int status,int destiny) {
/* Parameter s is only data. Status = 1 -> msg is token. Status = 2 -> msg is monitor. Status = 0 -> msg is data. */
    Message m;
    m.init = INIT;
    m.len = strlen(s);
    m.seq = Seq;
    Seq = (Seq++) % 256;
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
    int dataLength = len - NOTDATALENGTH;
    char *aux = s;
    m.data = malloc(dataLength + 1);
    memcpy(&m,s,5);
    aux += 5;
    memcpy(m.data,aux,dataLength);
    aux += dataLength;
    memcpy(&m.parity,aux,1);
    memcpy(&m.status,aux+1,1);
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

int rem_buffer(char **buf, int *len, int *first,int *destVec) {
/* This function will remove the first element from the buffer and update its length and first position. */
    free(buf[*first]); // Erasing data.
    destVec[*first] = -1;
    (*len)--;
    ((*first)++) % BUF_MAX; // If first is 1024, it will become 0 again.
    return 1;
}

int add_buffer(char **buf, int *len, int first, char *s,int dest,int *destVec) {
/* Parameters: Buffer, how many strings I have on the buffer and the string I will add in the buffer.
   I should add the string s to my buffer, so I can send it when I get the token.
   Returns 1 on success, 0 on failure (unable to allocate memory).
   PS: I dont have to use destLen neither destFirst, its the same position as buffer.*/
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
//        printf("Received %d bytes.\n",len);
    }
    RBegin.tv_sec = 0; // Got a message, someone has the token. I dont need my recovery timeout.
    int i;
    m = str_to_msg(s,len);
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
        gettimeofday(&TBegin, NULL); 
    } else { // Token sent, recovery timeout.
        gettimeofday(&RBegin, NULL);
    }
}

int timedout() {
/* Check if a timeout occured. If my Token timedout, return 1. */
    struct timeval now;
    gettimeofday(&now, NULL);
    long int diffSec = now.tv_sec - TBegin.tv_sec;
    if(Token == 1 && diffSec >= TOKEN_TIMEOUT/1000 && now.tv_usec - TBegin.tv_usec >= 0) { // Checking if my token timedout.
        TBegin.tv_usec = 0; // Reset my timer.
        return 1;
    }
    diffSec = now.tv_sec - RBegin.tv_sec;
    if(RBegin.tv_sec != 0 && diffSec >= RECOVERY_TIMEOUT/1000 && now.tv_usec - RBegin.tv_usec >= 0) { // Checking if recovery timedout.
        RBegin.tv_usec = 0;
        return 2;
    }
    return 0;
}

void print_message(Message m) {
    int type = typeof_msg(m);
    int isToken = (type == 1) ? 1 : 0;
    int isMonitor = (type == 2) ? 1 : 0;
    printf("Msg: Init = %d, Len = %d, Seq = %d, Dest = %d, Orig = %d, Data = %s, Parity = %d, Status = %d, Token = %d, Monitor = %d\n",
       (int)m.init,(int)m.len,(int)m.seq,(int)m.dest,(int)m.orig,m.data,(int)m.parity,(int)m.status,isToken,isMonitor);
}

int send_msg(Message m) {
/* This function should send the string s to my neighbor and return 1 on success and 0 on failure. */
    char *s = malloc(1024);
    s = msg_to_str(m);
    Seq = (Seq + 1) % 256;
    if(sendto(SockOut, s, m.len + NOTDATALENGTH, 0, (struct sockaddr *) &SocketC, sizeof(SocketC)+1) == -1)
        return 0;
    return 1;
}

int send_token() {
/* Send the token to the next machine. */
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
}

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
    Hosts = malloc(sizeof(char*) * 4);
    Hosts[0] = "bowmore"; // 1
    Hosts[1] = "cohiba"; // 2
    Hosts[2] = "achel"; // 3
    Hosts[3] = "latrappe"; // 4
    gethostname(localhost, MAX_HOSTNAME);

    if(argc != 2) {
        puts("Correct way to opearate: <MyMachine number>");
        return -1;
    }

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

    int machineInfo = 0;

    if(machineInfo)
        printf("MyMachine = %d, Localhost = %s, Port In = %d, Port Out = %d, Next = %d, Prev = %d\n",MyMachine,localhost,In,Out,Next,Prev);

    if((hp = gethostbyname(localhost)) == NULL) {
        puts("Couldn't get my own IP.");
        return -1;
    }

    create_server(hp);

    fds[1].fd = SockIn;
    fds[1].events = POLLIN|POLLPRI;

    puts("When all machines have set up the server, type any number to start connecting clients.");
    scanf("%d",&i);
    flush_buf();

    if(machineInfo)
        printf("The next machine name is: %s\n",Hosts[Next-1]);

    if((hp2 = gethostbyname(Hosts[Next-1])) == NULL) {
        puts("Couldn't get next machine IP.");
        return -1;
    }

    create_client(hp2);

    int gotMessage = 0;

    while(1) {
        if(Token == 1) {
            if(bufLen > 0) {
                expired = timedout();
                if(expired == 1) { // My token timed out =/.
                    send_token();
                    Token = 0;
                    gotMessage = 0;
                    if(poll(&(fds[1]),1,RECOVERY_TIMEOUT) == 0) {
                        Token = 1;
                        send_monitor();
                    } else {
                        receive_msg(m);
                        print_message(m);
                        gotMessage = 1;
                    }
                }
                if(destVec[bufFirst] == 0) { // User threw token away.
                    puts("Token thrown away.");
                    Token = 0;
                    gotMessage = 0;
                    if(poll(&(fds[1]),1,RECOVERY_TIMEOUT) == 0) {
                        Token = 1;
                        send_monitor();
                    } else {
                        receive_msg(m);
                        print_message(m);
                        gotMessage = 1;
                    }
                } else {
                    m = create_msg(buf[bufFirst],0,destVec[bufFirst]);
                    send_msg(m);
                }
                rem_buffer(buf,&bufLen,&bufFirst,destVec);
            } else {
                send_token();
                Token = 0;
            }
        }
        if(poll(fds,2,timeout_msecs) || gotMessage) {
            gotMessage = 0;
            if(fds[0].revents & POLLIN) { // There is something in stdin!
                fgets(s,1024,stdin);
                s = remove_enter(s);
                dest = s[0] - 48;
                s += 2; // To ignore the destiny and empty space bytes. ("2 " or "5 ").
                if(dest < -1 || dest > 5) {
                    puts("Format not known.");
                } else if(dest == 0) {
                    add_buffer(buf,&bufLen,bufFirst,"",0,destVec);
                } else {
                    add_buffer(buf,&bufLen,bufFirst,s,dest,destVec);
                }
                s -= 2; // Go back to original address.
            }
            if(fds[1].revents & POLLIN) {
                m = receive_msg();
                type = typeof_msg(m);
                if(type == TOKEN_TYPE) {
                    Token = 1;
                    set_timeout(1);
                } else if(type == 2) {
                    puts("Got a monitor!");
                    if(m.orig != MyMachine) {
                        send_msg(m);
                    }
                } else {
                    if(m.dest == MyMachine) {
                        printf("%s said to me: '%s'\n",Hosts[m.orig-1],m.data);
                    } else if(m.dest == 5 && m.orig != MyMachine) { // && will assure I will not print my own messages, only send them.
                        printf("%s said to all: '%s'\n",Hosts[m.orig-1],m.data);
                    }
                    if(m.orig != MyMachine) {
                        send_msg(m);
                    }
                }
            }
        }
    }
/*
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
            rem_buffer(buf,&bufLen,&bufFirst,destVec);
            if(timedout() == 1) { // Token time out.
                send_token();
                set_timeout(2); // 2 means recovery_timeout.
            }
        }
        if(poll(fds, 2, timeout_msecs) ) { // There is something to be read. Message or stdin.
                printf("entrou poll! algo para se ler \n");
                if(fds[0].revents & POLLIN|POLLPRI) { // Got something in STDIN.
                fgets(s,1025,stdin); // Is 1023 enough? See * (down there).
                int dest = s[0] - 48;
                if(dest < -1 || dest > 5) {
                    puts("Format not known.");
                } else {
                    if(dest == 0) {
                        puts("Throwing token away...");
                        Token = 0;
			set_timeout(2);
                    }
                    s += 2; // My data shall not contain the two first characters - machine number and space. ("3 ")
                    if(Token == 1) {
                        puts("I will be sending a message.");
                        Message m = create_msg(s,0,dest);
                        while(!send_msg(m)) { // Send a message with my string s.
                            puts("Problem sending message.");
                            printf("\tError was: %s\n",strerror(errno));
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
    }*/
}

/*
About typing format: Your message should be something like:
1 All the things I want to say to machine number 1.
It will be read in this way: 1 digit (In that example, 1), which is the machine number, a space and then text.
*/


/*
Os três inibidores destruídos pela Elite Azul, seguido das duas torres do Nexus e uma Riven fantástica de SkyBart, pareciam decretar o rebaixamento da KaBuM Black do CBLoL. No entanto, com uma luta fantástica onde Element flanqueou a Jayob com um Renekton imenso a equipe de Limeira sobreviveu e levou o terceiro jogo. E o quarto. E o quinto.

A equipe de SkyBart e Goku, ex-jogadores da KaBuM Black, começou muito bem a série, conquistando boas rotações e pressões de mapa. Turtle fazia o papel de tanque e utilidade do time junto de Baiano, e as rotas solo eram a principal fonte de dano da equipe - ajudada por um bom Mordekaiser de Pdr no segundo jogo. No entanto, a KaBuM desvendou o truque no terceiro. "Eles não mudaram o plano de jogo. Focavam o topo e faziam duas Pedras da Visão para controlar o mapa", disse Espeon. "O Veigar também era um problema maior do que esperávamos, então banimos ele. As rotas de baixo não faziam nada na partida, então priorizamos o Shen pra ter um jogador a mais no topo - que era o foco do jogo - ao mesmo tempo que não teríamos problemas no bot".
A vitória da KaBuM Black significou não só a vaga da equipe, mas de todas as participantes do CBLoL 2015 - Segunda Etapa. Jayob eSports, BigGods e Santos Dexterity voltam a integrar o Circuito Desafiante, onde tentarão buscar novamente a tão sonhada vaga para a elite do cenário competitivo.
E assim foi. Jogando duas vezes com Shen suporte, Espeon conseguiu ajudar muito sua equipe. Há de se destacar o Viktor fantástico de Vash durante a série (A/M/A total de 28/9/11 em três jogos), que segurou o ímpeto da Jayob com muito dano, principalmente na terceira partida, que iniciou a virada da equipe. O meio terminou com um placar de 17/4/6, sendo responsável por metade do dano de sua equipe, com 59,8 mil de dano mágico - veja o Histórico da Partida.
Os milhões de jogadores ao redor do mundo que jogam League of Legends geram um monte de dados. Se você visita sites que dão a média de vitórias e sumários de Campeões, você está vendo um pouco desses dados em ação. Como membro da equipe de Inteligência na Riot, ajudo a analisar esses dados puros em informação utilizável para que ela possa ser usada para melhorar League of Legends. Essa é a primeira parte de uma série recorrente sobre dados só pelo bem dos dados, chamada Clarividência.
Apesar de não darem nenhum atributos de combate, elas são incrivelmente poderosas. Sentinelas dão visão, visão é informação e informação é poder. Sentinelas permitem que você assuma menos riscos: menos emboscadas, menos gente indo de cara no arbusto ou perdendo o jogo no Barão.
Então sentinelas são boas, mas com que frequência jogadores normalmente as compram? No decorrer de uma partida, o jogador médio compra 0,9 sentinelas, que definimos aqui como Sentinelas Invisíveis ou Sentinelas Detectoras. Contudo, esses dados são salvos pelos heróis altruístas que compram várias sentinelas. A maioria dos jogadores, 64% deles, nunca comprou uma única sentinela e apenas um em dez compra mais de duas.
Não é surpreendente que as compras de sentinelas variam com as funções. Atiradores são os que compram menos sentinelas, sendo 0,3 por partida, enquanto suportes compram 1,8 sentinelas por jogo. Isso não inclui a Pedra da Visão, que predominantemente acaba na itemização dos suportes. Antes que você grite com a Vayne da sua equipe, tenha em mente que faz mais sentido comprar sentinelas em algumas funções que em outras. Suportes, por exemplo, não escalam tão bem com itens quanto as outras funções e normalmente investem mais de seu ouro em itens focados em equipe como as sentinelas. Caçadores normalmente passeiam mais pelo mapa e têm a oportunidade de colocar sentinelas em pontos estratégicos. Por outro lado, jogadores de topo ficam isolados de controles de objetivo de meio de partida, dependendo muito de sentinelas e podem gastar muito mais de seu ouro em itens de duelo.
Dito isso, a maioria dos jogadores provavelmente poderia gastar um pouco mais com sentinelas. Vamos dar uma olhada nos padrões de sentinelas dentre diferentes níveis de jogo. Jogadores no Bronze compram apenas 0,6 sentinelas por partida, enquanto jogadores no Mestre e Desafiante compram quatro vezes mais. A alta tendência é especialmente forte com Sentinelas Detectoras, que no Bronze é comprada menos de 0,2 vezes por partida, sendo que o Mestre típico compra mais de 1,2 por jogo. Aviso: apesar de poder existir um vínculo de causalidade entre a visão de seu jogo e sua classificação, dobrar as compras de sentinela provavelmente não fará com que você de repente pule do Prata para o Diamante. Essas tendências nos mostram correlação e não contam toda a história de quantas sentinelas você deveria comprar. Esse tipo de análise completa talvez seja um assunto para outro dia.
A maioria dos jogadores fica com o Totem da Vigilância sem aprimoramento. Especificamente, 73% dos jogadores acabam as partidas com o Amuleto amarelo, 12% com as Lentes Detectoras e 5% com uma das cinco opções.
Vamos condensar os Amuletos básicos e seus diferentes aprimoramentos em três classes: Esfera, Lentes e Totem. Agora, podemos ver mais facilmente as tendências entre funções e níveis de jogo diferentes.
Totem é o Amuleto escolhido para todas as funções exceto Suporte, que prefere as Lentes 54% das vezes. Caçadores ficam em segundo lugar na função que mais possui Lentes, escolhendo-as em 22% das partidas. A Esfera, sendo uma escolha mais de nicho e situacional, não recebe o mesmo tanto de amor. Apenas entre os atiradores a Esfera aparece em um número significativo o tempo todo, mas ainda assim, em apenas 18% das partidas.
O Totem continua dominante na maioria dos níveis de jogo, incluindo 91% dos jogos de Bronze e 50% de Diamante. A taxa de uso das Lentes e da Esfera crescem gradualmente quando você sobe na classificação, com as lentes ultrapassando os Totens do tier Mestre para cima. Lentes encontram mais ação nesses jogos de alta habilidade, em parte, porque jogadores nesse nível estão usando sentinelas com uma frequência muito maior. Faz sentido comprar uma vassoura se você tem algo a varrer!
Pode ser difícil criar o hábito de comprar sentinelas, mas siga nosso conselho: caso queira melhorar seu controle de visão, comece devagar. Pegue uma Sentinela Invisível a mais no primeiro retorno à base; você agradecerá por ter despendido 75 de ouro quando vir aquele caçador chegando. Pegue uma Sentinela Detectora antecipadamente e não sinta-se mal caso ela seja destruída. Não esqueça de aprimorar seu Amuleto; sim, é provavelmente melhor do que aquela quarta Espada Longa. Você irá mais longe que a maioria dos jogadores.








*/
