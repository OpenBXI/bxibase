all: client server

client: client.c
	gcc -o client client.c -lzmq

server: server.c
	gcc -o server server.c -lzmq


clean:
	rm client server
