CLIOBJ = common.o client.o
SRVOBJ = common.o server.o

CFLAGS = -g

common.o: common.c defs.h
	gcc $(CFLAGS) -c common.c -o common.o

client.o: client.c defs.h
	gcc $(CFLAGS) -c client.c -o client.o

server.o: server.c defs.h
	gcc $(CFLAGS) -c server.c -o server.o

cliente: $(CLIOBJ)
	gcc $(CFLAGS) $(CLIOBJ) -o cliente
	
servidor: $(SRVOBJ)
	gcc $(CFLAGS) $(SRVOBJ) -o servidor
