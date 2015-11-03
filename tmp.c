#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BUF_MAX 15

int add_buffer(char **buf, int *len, int first, char *s,int dest,int *destVec) {
/* Parameters: Buffer, how many strings I have on the buffer and the string I will add in the buffer.
   I should add the string s to my buffer, so I can send it when I get the token.
   Returns 1 on success, 0 on failure (unable to allocate memory).
   PS: I dont have to use destLen neither destFirst, its the same position as buffer.*/
    //puts("Adicionando no buffer...");
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

int rem_buffer(char **buf, int *len, int *first,int *destVec) {
/* This function will remove the first element from the buffer and update its length and first position. */
    free(buf[*first]); // Erasing data.
    destVec[*first] = -1;
    (*len)++;
    ((*first)++) % BUF_MAX; // If first is 1024, it will become 0 again.
    return 1;
}


int main() {
    int i,len=0,first=0,*vec;
    char *s;
    char **buf;
    buf = malloc(BUF_MAX * sizeof(char *));
    s = malloc(1024);
    vec = malloc(sizeof(int) * BUF_MAX);
    for(i=0; i<3; i++) {
        //puts("Pronto para ler.");
        scanf("%s",s);
        //puts("Li.");
        //printf("Li '%s'. Adicionando na tabela...\n");
        add_buffer(buf,&len,first,s,0,vec);
        //puts("Adicionado com sucesso no buffer.");
    }
    for(i=0; i<3; i++) {
        printf("%s - %d\n",buf[i],vec[i]);
    }
}
