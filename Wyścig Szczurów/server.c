/*
 * server.c
 *
 * Class responsible for maintaining clients
 * and overlooking general game strategy
 *
 *  Created on: 17 maj 2015
 *      Author: monika
 */
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "tcpConnect.h"
#include "client.h"

#define BACKLOG 3

struct Client clients[1000];

int getfromclient(int, char*, fd_set*);
int sendtoclient(int, char*, fd_set*);

int create_socket(int port)
{
	int s;
	struct sockaddr_in serv_addr;

	if ((s = socket(PF_INET, SOCK_STREAM, 0)) == -1)
		error("Cannot create socket");

	memset((void*) &serv_addr, 0x00, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons((short)port);
	serv_addr.sin_addr.s_addr = INADDR_ANY;

	if (bind(s, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) == -1)
		error("Cannot bind a name to a socket");
	if (listen(s, BACKLOG) == -1)
		error("Cannot listen for incoming connections");
	return s;

	return 0;
}

int addnewclient (int s, fd_set *mfds, int *fdmax)
{
	int fd;

	if ((fd = TEMP_FAILURE_RETRY(accept(s, NULL, NULL))) == -1)
		error("Cannot accept connection");

	FD_SET(fd, mfds);
	*fdmax = (*fdmax < fd) ? fd : *fdmax;
	return 1;
}

int deleteclient(int s, fd_set *mfds)
{
	if (TEMP_FAILURE_RETRY(close(s)) == -1)
		error("Cannot close socket");
	FD_CLR(s, mfds);
	return 1;
}

void clearallsockets(int fdmax, fd_set *mfds)
{
	int i;
	for (i = 0; i < fdmax + 1; ++i)
	{
		if (FD_ISSET(i, mfds))
			if (TEMP_FAILURE_RETRY(close(i)) == -1)
				error("Cannot close socket");
	}
}

int getfromclient(int s, char* buf, fd_set *mfds)
{
	int readb;

	readb = wread(s, &buf, strlen(buf));

	if (readb == 0)
	{
		if (TEMP_FAILURE_RETRY(close(s)) == -1)
			error("Cannot close socket");
		FD_CLR(s, mfds);
		return 1;
	}
	else
		return 0;
}

int sendtoclient(int s, char* buf, fd_set* mfds)
{
	int writb;
	fprintf(stderr, "Size of buf: %d", strlen(buf));
	writb = wwrite(s, buf, strlen(buf));

	if (writb == -1)
	{
		if (errno == EPIPE)
		{
			if (TEMP_FAILURE_RETRY(close(s)) == -1)
				error("Cannot close socket");
			FD_CLR(s, mfds);
			return 1;
		}
		else
			error("Cannot write to socket");
	}

	return 0;
}

void serverwork(int s)
{
	int i, clientcount = 0;
	int fdmax = s;
	fd_set mfds, curfds;

	FD_ZERO(&mfds);
	FD_SET(s, &mfds);

	while (work)
	{
		curfds = mfds;
		if (select(fdmax + 1, &curfds, NULL, NULL, NULL) == -1)
		{
			if (errno != EINTR) error ("Cannot select");
			else continue;
		}
		for (i = 0; i < fdmax + 1; ++i)
		{
			if (FD_ISSET(i, &curfds))
			{
				if (s == i)
				{
					clientcount += addnewclient(s, &mfds, &fdmax);
					fprintf(stderr, "Added new client\n");
					fprintf(stderr, "Current client count: %d\n", clientcount);
				}
				else
				{
					clientcount -= deleteclient(i, &mfds);
					fprintf(stderr, "Deleted client\n");
					fprintf(stderr, "Current client count: %d\n", clientcount);
				}
			}
		}
	}
	fprintf(stderr, " - SIGINT received. Closing program\n");
	FD_CLR(s, &mfds);
	clearallsockets(fdmax, &mfds);
}

void USAGE(char* name)
{
	fprintf(stderr, "USAGE:\n");
	fprintf(stderr, "%s port\n", name);
	fprintf(stderr, "port - port to listen on\n");
	exit(EXIT_FAILURE);
}

int main(int argc, char **argv)
{
	int s, port;

	if (argc != 2)
		USAGE(argv[0]);
	port = atoi(argv[1]);
	if (port < 1 || port > 65535)
		USAGE(argv[0]);

	work = 1;
	registerhandlers();
	s = create_socket(port);
	serverwork(s);

	if (TEMP_FAILURE_RETRY(close(s)) == -1)
		error("Cannot close socket");

	return EXIT_SUCCESS;
}



