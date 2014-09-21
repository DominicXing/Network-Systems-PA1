all: clean server client

server: udp_server.c
	gcc -Wall udp_server.c -o server

client: udp_client.c
	gcc -Wall udp_client.c -o client

clean:
	rm -f server client
