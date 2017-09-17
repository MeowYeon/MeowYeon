#include <stdio.h>
#include <stdlib.h>	//perror()
#include <string.h>
#include <sys/socket.h>
#include <netdb.h>	//getaddrinfo()
#include <signal.h>

#define MAX_BUF 4096
#define SERVER_PORT 12138

static void handle(int sig)
{
	exit(0);
}

int main(int argc, char *argv[])
{
	int sockfd;
	char sendBuf[MAX_BUF], recvBuf[MAX_BUF];
	int sendsize, recvsize;

	/*
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if(sockfd == -1)
	{
		perror("socket error: ");
		exit(1);
	}
	*/
	/*
	int optval = 1;
	setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
	*/

	if(argc < 3)
	{
		perror("use: ./client [hostname] [username]");
		exit(1);
	}
	
	struct sockaddr_in servaddr, cltaddr;
	struct addrinfo hints;
	struct addrinfo *result, *curr;

	bzero(&hints, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_flags = 0;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = 0;
	char port[6];
	bzero(port, 6);
	sprintf(port, "%d", SERVER_PORT);
	int status = getaddrinfo(argv[1], port, &hints, &result);
	if(status != 0)
	{
		fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
		exit(1);
	}

	for(curr = result; curr != NULL; curr = curr->ai_next)
	{
		sockfd = socket(curr->ai_family, curr->ai_socktype, curr->ai_protocol);
		if(sockfd == -1)
		{
			perror("socket error: ");
			exit(1);
		}
		int status = connect(sockfd, curr->ai_addr, curr->ai_addrlen);
		if(status != 0)//not fit, continue
		{
			perror("connect error: ");
			continue;
		}
		break;
	}
	if(curr == NULL)
	{
		printf("get addr of %s\n", argv[1]);
		printf("there is not a fit addr\n");
		exit(1);
	}

	freeaddrinfo(result);

	int pid = fork();
	switch(pid)
	{
	case -1://error
		perror("fork error: ");
		exit(1);
	case 0://child
		signal(SIGUSR1, handle);
		while(1)
		{
			bzero(recvBuf, MAX_BUF);
			if((recvsize = recv(sockfd, recvBuf, MAX_BUF, 0)) == -1)
			{
				perror("recv error: ");
				exit(1);
			}
			printf("[recv]: %s", recvBuf, recvsize);
			printf("recv %d bytes\n", recvsize);

			if(strcmp(recvBuf, "bye\n") == 0)
			{
				close(sockfd);
				exit(0);
			}
		}
		break;
	default:
		while(1)
		{
			bzero(sendBuf, MAX_BUF);
			printf("[send]: ");
			fgets(sendBuf, MAX_BUF, stdin);
			//strlen is nessary
			if((sendsize = send(sockfd, sendBuf, strlen(sendBuf), 0)) == -1)
			{
				perror("send error: ");
				exit(1);
			}
			printf("send %d bytes\n", sendsize);
			if(strcmp(sendBuf, "bye\n") == 0)
			{
				close(sockfd);
				kill(pid, SIGUSR1);
				exit(0);
			}
		}
		break;
	}

	close(sockfd);
	exit(0);
}
