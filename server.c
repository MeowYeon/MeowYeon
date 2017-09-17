#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
//共享内存
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
//epoll
#include <sys/epoll.h>

#define MAX_BUF 4096
#define SERVER_PORT 12138
#define BACKLOG 5
#define MAX_CONN 20
#define EVENT_LIST 20
#define MAX_EPOLL 256

int main(int argc, char *argv[])
{
	int sockfd, cfd;
	char sendBuf[MAX_BUF], recvBuf[MAX_BUF];
	int sendsize, recvsize;

	struct addrinfo hints;
	struct addrinfo *result, *curr;
	bzero(&hints, sizeof(hints));
	hints.ai_flags = AI_PASSIVE;
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = 0;
	char port[6];
	bzero(port, sizeof(port));
	sprintf(port, "%d", SERVER_PORT);
	int status = getaddrinfo(NULL, port, &hints, &result);
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
			continue;
		}
		int optval = 1;
		if(setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) == -1)
		{
			perror("setsockopt error: ");
			exit(1);
		}
		
		if(bind(sockfd, curr->ai_addr, curr->ai_addrlen) == 0)
		{
			break;
		}
		perror("bind error:");
		close(sockfd);
	}
	if(curr == NULL)
	{
		perror("there is not a fit addr");
		exit(1);
	}
	freeaddrinfo(result);

	if(listen(sockfd, BACKLOG) != 0)
	{
		perror("listen error: ");
		exit(1);
	}

	struct epoll_event ev, events[EVENT_LIST];
	int epfd = epoll_create(MAX_EPOLL);
	ev.data.fd = sockfd;
	ev.events = EPOLLIN|EPOLLET;
	epoll_ctl(epfd, EPOLL_CTL_ADD, sockfd, &ev);

	int fd = shm_open("shared", O_CREAT|O_RDWR, S_IRWXU);
	if(fd == -1)
	{
		perror("shm_open error: ");
		exit(1);
	}
	if(ftruncate(fd, sizeof(int)*MAX_CONN) == -1)
	{
		perror("ftruncate error: ");
		exit(1);
	}
	int *conn = (int *)mmap(NULL, sizeof(int)*MAX_CONN, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
	int count = 0;
	
	while(1)
	{
		struct sockaddr_in clt;
		socklen_t len = sizeof(struct sockaddr_in); //sizeof(struct sockaddr_in) e, i don't know why it work just for the first connection, now it's well
		int nfds = epoll_wait(epfd, events, EVENT_LIST, -1);
		int i = 0;
		int flag = 0;
		for(i = 0; i < nfds; ++i)
		{
			if(events[i].data.fd == sockfd)
			{
				cfd = accept(sockfd, (struct sockaddr *)&clt, &len);
				if(cfd == -1)
				{
					perror("accept error: ");
					exit(1);
				}
				conn[count++] = cfd;
				ev.events = EPOLLIN|EPOLLET;
				ev.data.fd = cfd;
				epoll_ctl(epfd, EPOLL_CTL_ADD, cfd, &ev);

				//send msg to all of the connection
				bzero(sendBuf, MAX_BUF);
				char cltname[INET_ADDRSTRLEN];
				bzero(cltname, INET_ADDRSTRLEN);
				if(inet_ntop(AF_INET, &clt.sin_addr, cltname, INET_ADDRSTRLEN) == NULL)
				{
					perror("inet_ntop error: ");
					exit(1);
				}

				snprintf(sendBuf, MAX_BUF, "%s %s\n", "welcome new connection from", cltname);
				int j = 0;
				for(j = 0; j < count; ++j)
				{
					sendsize = send(conn[j], sendBuf, strlen(sendBuf), 0);
					if(sendsize == -1)
					{
						perror("send error: ");
						exit(1);
					}
				}
			}
			else if(events[i].events & EPOLLIN)
			{
				cfd = events[i].data.fd;
				bzero(recvBuf, MAX_BUF);
				if((recvsize = recv(cfd, recvBuf, MAX_BUF, 0)) == -1)
				{
					perror("recv error: ");
					exit(1);	
				}
				printf("[recv]: %s", recvBuf);
				printf("recv %d bytes\n", recvsize);
				
				if(strcmp(recvBuf, "bye\n") == 0)
				{
					epoll_ctl(epfd, EPOLL_CTL_DEL, cfd, NULL);
					close(cfd);
					conn[--count] = 0;
					flag = 1;
					continue;
				}
				ev.data.fd = cfd;
				ev.events = EPOLLOUT|EPOLLET;
				epoll_ctl(epfd, EPOLL_CTL_MOD, cfd, &ev);
			}
			else if(events[i].events & EPOLLOUT)
			{
				cfd = events[i].data.fd;
				if(flag)
				{
					bzero(sendBuf, MAX_BUF);
					char cltname[INET_ADDRSTRLEN];
					bzero(cltname, INET_ADDRSTRLEN);
					if(inet_ntop(AF_INET, &clt.sin_addr, cltname, INET_ADDRSTRLEN) == NULL)
					{
						perror("inet_ntop error: ");
						exit(1);
					}

					snprintf(sendBuf, MAX_BUF, "%s %s\n", "the left connection is from", cltname);
					int j = 0;
					for(j = 0; j < count; ++j)
					{
						sendsize = send(conn[j], sendBuf, strlen(sendBuf), 0);
						if(sendsize == -1)
						{
							perror("send error: ");
							exit(1);
						}
					}
					flag = 0;
					continue;
				}
				bzero(sendBuf, MAX_BUF);
				snprintf(sendBuf, MAX_BUF, "%s\n", "reply");
				if((sendsize = send(cfd, sendBuf, strlen(sendBuf), 0)) == -1)
				{
					perror("send error: ");
					exit(1);
				}
				ev.events = EPOLLIN|EPOLLET;
				ev.data.fd = cfd;
				epoll_ctl(epfd, EPOLL_CTL_MOD, cfd, &ev);
			}
		}
	}

	shm_unlink("shared");
	close(sockfd);
	exit(0);
}
