     # Diretorio base onde estarão os diretórios de biblioteca
    PREFIX = ./

     # Arquivo de teste
    TEST = connection

    CC = gcc -g
    AR = ar -rcu
    INSTALL = install

.PHONY:  clean distclean install all

%.o:  %.c
	$(CC) -c $(CFLAGS) $<

all: install $(TEST).o

$(TEST).o: $(TEST).c
	$(CC) -o $(PREFIX)$(TEST) $(TEST).c

limpa:
	@rm -f *% *.bak *~

faxina:   limpa
	@rm -rf *.o $(TEST)
