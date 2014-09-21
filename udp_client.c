#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/time.h>
#include <stdlib.h>
#include <memory.h>
#include <errno.h>

#define MAXBUFSIZE 100

int main (int argc, char * argv[])
{
	int sock;                               //this will be our socket
	char buffer[MAXBUFSIZE];
	char command[MAXBUFSIZE];
	char filename[MAXBUFSIZE];
	FILE* file_pointer;
	int keep_going = 1;
	char* eof = "EOF";

	struct sockaddr_in remote, from_addr;              //"Internet socket address structure"
    int addr_length = sizeof(struct sockaddr);

	if (argc < 3)
	{
		printf("USAGE:  <server_ip> <server_port>\n");
		exit(1);
	}

	/******************
	  Here we populate a sockaddr_in struct with
	  information regarding where we'd like to send our packet 
	  i.e the Server.
	 ******************/
	bzero(&remote,sizeof(remote));               //zero the struct
	remote.sin_family = AF_INET;                 //address family
	remote.sin_port = htons(atoi(argv[2]));      //sets port to network byte order
	remote.sin_addr.s_addr = inet_addr(argv[1]); //sets remote IP address

	//Causes the system to create a generic socket of type UDP (datagram)
	if ((sock = socket(remote.sin_family, SOCK_DGRAM, 0)) < 0)
	{
		printf("Unable to create socket.");
	}

	bzero(filename, MAXBUFSIZE);
	
	while(keep_going == 1) {
		
		// Grab the command
		bzero(command, sizeof(command));
		printf("Command? \n");
		gets(command);
		
		// Send the command
		sendto(sock, command, sizeof(command), 0, (struct sockaddr *)&remote, addr_length);
		//printf("COMMAND %s SENT\n", command);
		// Pong response of command - this confirms server got it and knows what to do
		bzero(buffer, sizeof(buffer));
		recvfrom(sock, buffer, sizeof(buffer), 0, (struct sockaddr *)&from_addr, (socklen_t *)&addr_length);
		
		if(strncmp(buffer, "receiving file", 14) == 0)
		{
		  	bzero(filename, sizeof(filename));
		  	printf("Receiving file.\n");
		  	recvfrom(sock, filename, sizeof(filename), 0, (struct sockaddr *)&from_addr, (socklen_t *)&addr_length);
		  	file_pointer = fopen(filename, "w");
		  	
		  	printf("Opened file %s for local writing.\n", filename);
		  	int count = 0;
		  	while(strncmp(buffer, "EOF", 3) != 0)
		  	{
				bzero(buffer, MAXBUFSIZE);
				recvfrom(sock, buffer, sizeof(buffer), 0, (struct sockaddr *)&from_addr, (socklen_t *)&addr_length);
				printf("Received chunk %d\n", count);
				if (strncmp(buffer, "EOF", 3) != 0 )
				{
					fwrite(buffer, MAXBUFSIZE, 1, file_pointer);
					//printf("%s", buffer);
				}
				count++;
			}
			printf("File transfer complete.\n");
			fclose(file_pointer);
		}
		else if(strncmp(buffer, "putting file", 12) == 0)
		{
			bzero(filename, sizeof(filename));
		  	recvfrom(sock, filename, sizeof(filename), 0, (struct sockaddr *)&from_addr, (socklen_t *)&addr_length);
			
			file_pointer = fopen(filename, "r");
			
			if(file_pointer == NULL)
			{
				fprintf(stderr, "Cannot open input file %s!\n", filename);
				return EXIT_FAILURE;
			}
			
			printf("Opened file %s for local reading.\n", filename);
			
			int count = 0;
			int server_count = 0;
			char chunk[MAXBUFSIZE];
			bzero(chunk, MAXBUFSIZE);
			while(!feof(file_pointer))
			{
				printf("Sending chunk %d\n", count);
				fread(chunk, MAXBUFSIZE, 1, file_pointer);
				sendto(sock, chunk, sizeof(chunk), 0, (struct sockaddr *)&from_addr, (socklen_t)addr_length);
				recvfrom(sock, &server_count, sizeof(server_count), MSG_DONTWAIT, (struct sockaddr *)&from_addr, (socklen_t *)&addr_length);
				while(server_count != count)
				{
					printf("Server: %d - Client: %d\n", server_count, count);
					sendto(sock, chunk, sizeof(chunk), 0, (struct sockaddr *)&from_addr, (socklen_t)addr_length);
					recvfrom(sock, &server_count, sizeof(server_count), MSG_DONTWAIT, (struct sockaddr *)&from_addr, (socklen_t *)&addr_length);
				}
				bzero(chunk, MAXBUFSIZE);
				count++;
			}
			sendto(sock, eof, 3, 0, (struct sockaddr *)&from_addr, (socklen_t)addr_length);
			fclose(file_pointer);
		}
		else if(strncmp(buffer, "ls", 2) == 0)
		{
			while(strncmp(buffer, "EOF", 3) != 0)
			{
			   bzero(buffer, sizeof(buffer));
			   recvfrom(sock, buffer, sizeof(buffer), 0, (struct sockaddr *)&from_addr, (socklen_t *)&addr_length);
			   if (strncmp(buffer, "EOF", 3) != 0 )
			   {
				   printf("%s", buffer);
				   
			   }
			   else
			   {
				   //printf("EOF detected.\n");
			   }
			}
		}
		else if(strncmp(buffer, "exit", 4) == 0)
		{
			keep_going = 0;
		}
		//printf("Keep going? %d\n", keep_going);		
	}	


	close(sock);
	return EXIT_SUCCESS;
}

