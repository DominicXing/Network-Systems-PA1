#include <sys/types.h>
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
#include <string.h>

#define MAXBUFSIZE 100
char *substring(char *string, int position, int length);
int execute_command(int sock, struct sockaddr_in sin, char * command);

int main (int argc, char * argv[] )
{
	int sock;                           //This will be our socket
	struct sockaddr_in sin, remote;     //"Internet socket address structure"
	unsigned int remote_length;         //length of the sockaddr_in structure
	int nbytes;                        //number of bytes we receive in our message
	char buffer[MAXBUFSIZE];             //a buffer to store our received message
	if (argc != 2)
	{
		printf ("USAGE:  <port>\n");
		exit(1);
	}

	/******************
	  This code populates the sockaddr_in struct with
	  the information about our socket
	 ******************/
	bzero(&sin,sizeof(sin));                    //zero the struct
	sin.sin_family = AF_INET;                   //address family
	sin.sin_port = htons(atoi(argv[1]));        //htons() sets the port # to network byte order
	sin.sin_addr.s_addr = INADDR_ANY;           //supplies the IP address of the local machine

    bzero(&remote, sizeof(remote));

	//Causes the system to create a generic socket of type UDP (datagram)
	if ((sock = socket(sin.sin_family, SOCK_DGRAM, 0)) < 0)
	{
		printf("Unable to create socket.\n");
		exit(EXIT_FAILURE);
	}

	if (bind(sock, (struct sockaddr *)&sin, sizeof(sin)) < 0)
	{
		printf("Unable to bind socket.\n");
		exit(EXIT_FAILURE);
	}

	remote_length = sizeof(remote);
	int status = 0;

	while(1) {
		//waits for an incoming message
		bzero(buffer, sizeof(buffer));
		nbytes = recvfrom(sock, buffer, sizeof(buffer), 0, (struct sockaddr *)&remote, &remote_length);
		//printf("Received packet from %s:%d\nData: %s\n\n", inet_ntoa(remote.sin_addr), ntohs(remote.sin_port), buffer);

        if(nbytes <= 0 || strncmp(buffer, "\0", 1) == 0)
        {
			printf("Empty message received.\n");
			//do nothing - basic check
		}
		else
		{
			printf("The client says %s\n", buffer);

			status = execute_command(sock, remote, buffer);
			if(status == EXIT_FAILURE) {
				printf("There was a problem with a received command.\n");
				break;
			}
		}
	}

	close(sock);
	return EXIT_SUCCESS;
}

int execute_command(int sock, struct sockaddr_in sin, char * command)
{
	char* filename;
	char chunk[MAXBUFSIZE];
	char* eof = "EOF";
	bzero(chunk, MAXBUFSIZE);
	FILE *file_pointer;

	//“get [file_name]”: The server transmits the requested file to the client (use files of small size in order of 2 to 5 KB for transfer like any jpeg file).
	if(strstr(command, "get ") != NULL) {
		// Grab everything after "get " and put it into filename
		filename = substring(command, 5, strlen(command)-4);
		
		file_pointer = fopen(filename, "r");
        
		if (file_pointer == NULL)
		{
		  fprintf(stderr, "Cannot open input file %s!\n", filename);
		  return EXIT_FAILURE;
		}
		
		char* reply = "receiving file";
		sendto(sock, reply, 14, 0, (struct sockaddr *)&sin, sizeof(sin));
		sendto(sock, filename, sizeof(filename), 0, (struct sockaddr *)&sin, sizeof(sin));
		int count=0;
		while(!feof(file_pointer))
		{
			//printf("Sending chunk %d\n", count);
			fread(chunk, MAXBUFSIZE, 1, file_pointer);
			sendto(sock, chunk, sizeof(chunk), 0, (struct sockaddr *)&sin, sizeof(sin));
			bzero(chunk, MAXBUFSIZE);
			count++;
		}
		sendto(sock, eof, 3, 0, (struct sockaddr *)&sin, sizeof(sin));
		fclose(file_pointer);
		free(filename);
		return EXIT_SUCCESS;
	}

	//“put [file_name]”: The server receives the transmitted file by the client and stores it locally (use files of small size in order of 2 to 5 KB for transfer like any jpeg file).
	else if(strstr(command, "put ") != NULL) {
		// Grab everything after "put " and put it into filename
		filename = substring(command, 5, strlen(command)-4);
		//printf("PUT %s\n", filename);

		file_pointer = fopen(filename, "w");//send filename
		printf("Opened file %s for writing.\n", filename);
		char* reply = "putting file";
		sendto(sock, reply, 12, 0, (struct sockaddr *)&sin, sizeof(sin));
		sendto(sock, filename, sizeof(filename), 0, (struct sockaddr *)&sin, sizeof(sin));
		int count = 0;
		char buffer[MAXBUFSIZE];
		bzero(buffer, MAXBUFSIZE);
		while(strncmp(buffer, "EOF", 3) != 0)
		{
			bzero(buffer, MAXBUFSIZE);
			recvfrom(sock, buffer, sizeof(buffer), 0, (struct sockaddr *)&sin, (socklen_t *)sizeof(sin));
			printf("Received chunk %d\n", count);
			//Confirmation packet
			sendto(sock, &count, sizeof(count), 0, (struct sockaddr *)&sin, (socklen_t)sizeof(sin));
			
			if (strncmp(buffer, "EOF", 3) != 0 )
			{
				fwrite(buffer, MAXBUFSIZE, 1, file_pointer);
				//printf("%s", buffer);
			}
			count++;
		}
		printf("File closed.\n");
		fclose(file_pointer);
		free(filename);
		return EXIT_SUCCESS;
	}

	//“ls”: The server should search all the files it has in its local directory and send a list of all these files to the client.
	else if(strncmp(command, "ls", 2) == 0) {
		
		/* Open the command for reading. */
		file_pointer = popen("/bin/ls", "r");
		if (file_pointer == NULL) {
			printf("Failed to run command 'ls'\n" );
			return EXIT_FAILURE;
		}
		/* Read the output a line at a time - output it. */
		char* reply = "ls";
		sendto(sock, reply, sizeof(reply), 0, (struct sockaddr *)&sin, sizeof(sin));
		while (!feof(file_pointer)) {
			fgets(chunk, sizeof(chunk)-1, file_pointer);
			//printf("%s", chunk);
			sendto(sock, chunk, sizeof(chunk), 0, (struct sockaddr *)&sin, sizeof(sin));
			bzero(chunk, MAXBUFSIZE);
		}
		sendto(sock, eof, 3, 0, (struct sockaddr *)&sin, sizeof(sin));
		/* close input stream */
		pclose(file_pointer);
		
		return EXIT_SUCCESS;
	}

	//“exit” : The server should exit gracefully.
	else if(strncmp(command, "exit", 4) == 0)
	{
		printf("Exiting gracefully.\n");
		sendto(sock, "exit", 4, 0, (struct sockaddr *)&sin, sizeof(sin));
		close(sock);
		exit(EXIT_SUCCESS);
		return EXIT_SUCCESS; //unreachable - jic.
	}
    else
    {
		// For any other commands, the server should simply repeat the command back to the client with no modification, stating that the given command was not understood.
		char reply[MAXBUFSIZE];
		bzero(reply, sizeof(reply));
		strcat(reply, "Unrecognized command: ");
		strcat(reply, command);
		strcat(reply, "\n");
		printf("Unrecognized command: %s\n", command);
		sendto(sock, reply, sizeof(reply), 0, (struct sockaddr *)&sin, sizeof(sin));
		return EXIT_FAILURE;	
	}
	return EXIT_FAILURE; //unreachable - jic.
}

/* Based on code snippet from http://www.programmingsimplified.com/c/source-code/c-substring */
/* C substring function: It returns a pointer to the substring */ 
char *substring(char *string, int position, int length) 
{
   char *pointer;
   int c;

   pointer = malloc(length+1);
   
   if (pointer == NULL)
   {
      printf("Unable to allocate memory.\n");
      exit(EXIT_FAILURE);
   }
 
   for (c = 0 ; c < position -1 ; c++) 
      string++; 
 
   for (c = 0 ; c < length ; c++)
   {
      *(pointer+c) = *string;      
      string++;   
   }
 
   *(pointer+c) = '\0';
 
   return pointer;
}
