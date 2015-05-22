/*
 * server.c
 *
 * Class responsible for maintaining clients
 * and overlooking general game strategy
 *
 *  Created on: 17 maj 2015
 *      Author: Monika Żurkowska
 */
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>

#include "tcpConnect.h"
#include "client.h"

#define BACKLOG 3

int getfromclient(int, char*, fd_set*);
int sendtoclient(int, char*, fd_set*);
int deleteclient(int, fd_set*, struct GameInfo*);

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

int addnewclient (int s, fd_set *mfds, int *fdmax, struct GameInfo* gameInfo)
{
	int fd, i;
	char buf[100];
	int found = 0;

	if ((fd = TEMP_FAILURE_RETRY(accept(s, NULL, NULL))) == -1)
		error("Cannot accept connection");

	FD_SET(fd, mfds);
	*fdmax = (*fdmax < fd) ? fd : *fdmax;

	safemutexlock(&(gameInfo->numOfClients_mutex));
	gameInfo->numOfClients++;
	safemutexunlock(&(gameInfo->numOfClients_mutex));

	safemutexlock(&(gameInfo->numOfClients_mutex));
	if (gameInfo->numOfClients >= MAX_CLIENT)
	{
		safemutexunlock(&(gameInfo->numOfClients_mutex));
		if(snprintf(buf, 100, "We're sorry, to many log in clients. Try again later") == -1)
			error("Cannot initialize buffer");
		sendtoclient(fd, buf, mfds);
		deleteclient(fd, mfds, gameInfo);
		return 0;
	}
	else
		safemutexunlock(&(gameInfo->numOfClients_mutex));

	i = fd%MAX_CLIENT;
	safemutexlock(&(gameInfo->clients_mutex[i]));
	if (gameInfo->clients[i] != NULL)
	{
		safemutexunlock(&(gameInfo->clients_mutex[i]));
		while ((i = (i+1)%MAX_CLIENT) != fd%MAX_CLIENT)
		{
			safemutexlock(&(gameInfo->clients_mutex[i]));
			if (gameInfo->clients[i] == NULL)
			{
				found = 1;
				break;
			}
			if (!found)
				safemutexunlock(&(gameInfo->clients_mutex[i]));
		}
	}

	gameInfo->clients[i]->fd = fd;
	gameInfo->clients[i]->index = i;
	gameInfo->clients[i]->rank = 0;
	safemutexunlock(&(gameInfo->clients_mutex[i]));

	if (snprintf(buf, 20, "Your nick is: %d", fd) == -1)
		error("Cannot initialize buffer");

	sendtoclient(fd, buf, mfds);

	return 1;
}

int deleteclient(int s, fd_set *mfds, struct GameInfo* gameInfo)
{
	int i;
	int found = 0;
	if (TEMP_FAILURE_RETRY(close(s)) == -1)
		error("Cannot close socket");
	FD_CLR(s, mfds);

	i = s%MAX_CLIENT;
	safemutexlock(&(gameInfo->clients_mutex[i]));
	if (gameInfo->clients[i] == NULL || gameInfo->clients[i]->fd%MAX_CLIENT != gameInfo->clients[i]->index)
	{
		safemutexunlock(&(gameInfo->clients_mutex[i]));
		while ((i = (i+1)%MAX_CLIENT) != s%MAX_CLIENT)
		{
			safemutexlock(&(gameInfo->clients_mutex[i]));
			if (gameInfo->clients[i] != NULL && gameInfo->clients[i]->fd == s)
			{
				found = 1;
				break;
			}
			if (!found)
				safemutexunlock(&(gameInfo->clients_mutex[i]));
		}
	}
	gameInfo->clients[i] = NULL;
	safemutexunlock(&(gameInfo->clients_mutex[i]));

	safemutexlock(&(gameInfo->numOfClients_mutex));
	gameInfo->numOfClients--;
	safemutexunlock(&(gameInfo->numOfClients_mutex));

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

void* serverwork(void* arg/*, struct GameInfo* gameInfo*/)
{
	struct ServerThreadParams* stp = (struct ServerThreadParams*)arg;
	int s = stp->s;
	struct GameInfo* gameInfo = stp->gameInfo;
	int i, clientcount = 0;
	int fdmax = s;
	fd_set mfds, curfds;

	FD_ZERO(&mfds);
	FD_SET(s, &mfds);
	fprintf(stderr, "Listening for clients....\n");

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
					clientcount += addnewclient(s, &mfds, &fdmax, gameInfo);
					fprintf(stderr, "Added new client\n");
					fprintf(stderr, "Current client count: %d\n", clientcount);
				}
				else
				{
					clientcount -= deleteclient(i, &mfds, gameInfo);
					fprintf(stderr, "Deleted client\n");
					fprintf(stderr, "Current client count: %d\n", clientcount);
				}
			}
		}
	}
	fprintf(stderr, " - SIGINT received. Closing program\n");
	FD_CLR(s, &mfds);
	clearallsockets(fdmax, &mfds);
	return (void*)0;
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
	int s, port, i;
	pthread_t tid;
	void* retval;
	sigset_t mask;
	sigemptyset(&mask);
	sigaddset(&mask, SIGINT);

	if (argc != 2)
		USAGE(argv[0]);
	port = atoi(argv[1]);
	if (port < 1 || port > 65535)
		USAGE(argv[0]);

	if ((gameInfo.clients = (struct Client**)calloc(MAX_CLIENT, sizeof(struct Client*))) == NULL)
		error("Cannot allocate memory for Client array");
	if ((gameInfo.games = (int**)calloc(MAX_CLIENT, sizeof(int*))) == NULL)
		error("Cannot allocate memory of games array");
	for (i = 0; i < MAX_CLIENT; ++i)
	{
		pthread_mutex_init(&(gameInfo.clients_mutex[i]), NULL);
		if ((gameInfo.clients[i] = (struct Client*)malloc(sizeof(struct Client))) == NULL)
			error("Cannot allocate memory for Client array");
		if ((gameInfo.games[i] = (int*)malloc(sizeof(int))) == NULL)
			error("Cannot allocate memory for games array");
	}
	gameInfo.numOfClients = 0;
	pthread_mutex_init(&(gameInfo.numOfClients_mutex), NULL);
	//pthread_mutex_init(&(gameInfo.clients_mutex), NULL);

	work = 1;
	registerhandlers();
	s = create_socket(port);
	fprintf(stderr, "Created Server socket\n");
	serverThreadParams.s = s;
	serverThreadParams.gameInfo = &gameInfo;
	pthread_create(&tid, NULL, serverwork, &serverThreadParams);
	//serverwork(s/*, &gameInfo*/);
	if (pthread_sigmask(SIG_BLOCK, &mask, NULL))
		error("Main: Cannot block SIGINT");
	pthread_join(tid, &retval);

	if (TEMP_FAILURE_RETRY(close(s)) == -1)
		error("Cannot close socket");
	for (i = 0; i < MAX_CLIENT; ++i)
		free(gameInfo.clients[i]);
	free(gameInfo.clients);

	return EXIT_SUCCESS;
}



