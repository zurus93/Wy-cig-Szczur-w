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
#include <string.h>

#include "tcpConnect.h"
#include "client.h"

#define BACKLOG 3

int deleteclient(int, fd_set* mfds, struct GameInfo*);
int factorial(int);
void sendranking(struct GameInfo* gi);

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

int addnewclient (int s, fd_set* mfds, int *fdmax, struct GameInfo* gameInfo)
{
	int fd, index;
	char buf[100];
	int found = 0;

	if ((fd = TEMP_FAILURE_RETRY(accept(s, NULL, NULL))) == -1)
		error("Cannot accept connection");

	FD_SET(fd, mfds);
	*fdmax = (*fdmax < fd) ? fd : *fdmax;

	safemutexlock(&(gameInfo->numOfClients_mutex));
	if (gameInfo->numOfClients >= MAX_CLIENT)
	{
		safemutexunlock(&(gameInfo->numOfClients_mutex));
		if(snprintf(buf, 100, "We're sorry, to many log in clients. Try again later") == -1)
			error("Cannot initialize buffer");
		wwrite(fd, buf, strlen(buf));
		deleteclient(fd, mfds, gameInfo);
		return 0;
	}
	else
	{
		gameInfo->numOfClients++;
		safemutexunlock(&(gameInfo->numOfClients_mutex));
	}

	safemutexlock(&(gameInfo->numOfGames_mutex));
	gameInfo->numOfGames = factorial(gameInfo->numOfClients);
	safemutexunlock(&(gameInfo->numOfGames_mutex));

	index = fd%MAX_CLIENT;
	safemutexlock(&(gameInfo->clients_mutex[index]));
	if (gameInfo->clients[index] != NULL)
	{
		safemutexunlock(&(gameInfo->clients_mutex[index]));
		while ((index = (index+1)%MAX_CLIENT) != fd%MAX_CLIENT)
		{
			safemutexlock(&(gameInfo->clients_mutex[index]));
			if (gameInfo->clients[index] == NULL)
			{
				found = 1;
				break;
			}
			if (!found)
				safemutexunlock(&(gameInfo->clients_mutex[index]));
		}
	}

	gameInfo->clients[index]->fd = fd;
	gameInfo->clients[index]->indeks = index;
	gameInfo->clients[index]->rank = 0;
	safemutexunlock(&(gameInfo->clients_mutex[index]));

	if (snprintf(buf, 20, "Your nick is: %d\n", fd) == -1)
		error("Cannot initialize buffer");

	wwrite(fd, buf, strlen(buf));
	pthread_cond_signal(&(gameInfo->changeInClient));

	return 1;
}

int deleteclient(int s, fd_set* mfds, struct GameInfo* gameInfo)
{
	int i, index;
	int found = 0;
	if (TEMP_FAILURE_RETRY(close(s)) == -1)
		error("Cannot close socket");
	FD_CLR(s, mfds);

	index = s%MAX_CLIENT;
	safemutexlock(&(gameInfo->clients_mutex[index]));
	if (gameInfo->clients[index] == NULL || gameInfo->clients[index]->fd%MAX_CLIENT != gameInfo->clients[index]->indeks)
	{
		safemutexunlock(&(gameInfo->clients_mutex[index]));
		while ((index = (index+1)%MAX_CLIENT) != s%MAX_CLIENT)
		{
			safemutexlock(&(gameInfo->clients_mutex[index]));
			if (gameInfo->clients[index] != NULL && gameInfo->clients[index]->fd == s)
			{
				found = 1;
				break;
			}
			if (!found)
				safemutexunlock(&(gameInfo->clients_mutex[index]));
		}
	}
	safemutexlock(&(gameInfo->numOfPlayedGames_mutex));
	gameInfo->numOfPlayedGames -= gameInfo->clients[index]->numOfPlayedGames;
	safemutexunlock(&(gameInfo->numOfPlayedGames_mutex));
	gameInfo->clients[index]->fd = 0;
	gameInfo->clients[index]->indeks = 0;
	gameInfo->clients[index]->numOfPlayedGames = 0;
	gameInfo->clients[index]->rank = 0;
	safemutexunlock(&(gameInfo->clients_mutex[index]));

	safemutexlock(&(gameInfo->numOfClients_mutex));
	gameInfo->numOfClients--;
	safemutexunlock(&(gameInfo->numOfClients_mutex));

	safemutexlock(&(gameInfo->numOfGames_mutex));
	gameInfo->numOfGames = factorial(gameInfo->numOfClients);
	safemutexunlock(&(gameInfo->numOfGames_mutex));

	for (i = 0; i < MAX_CLIENT; ++i)
	{
		gameInfo->games[index][i] = 0;
		gameInfo->games[i][index] = 0;
	}
	safemutexunlock(&(gameInfo->games_mutex));

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

int factorial(int n)
{
	if (n == 1 || n == 0)
		return 0;
	int f = 0;
	while (--n > 0)
		f += n;
	return f;
}

void* serverwork(void* arg)
{
	struct ServerThreadParams* stp = (struct ServerThreadParams*)arg;
	int s = stp->s;
	struct GameInfo* gameInfo = stp->gameInfo;
	int i, clientcount = 0;
	int fdmax = s;
	fd_set curfds, mfds;

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
					fflush(NULL);
				}
				else
				{
					clientcount -= deleteclient(i, &mfds, gameInfo);
					fprintf(stderr, "Deleted client\n");
					fprintf(stderr, "Current client count: %d\n", clientcount);
					fflush(NULL);
				}
			}
		}
	}
	fprintf(stderr, " - SIGINT received. Closing program\n");
	FD_CLR(s, &mfds);
	clearallsockets(fdmax, &mfds);
	return (void*)EXIT_SUCCESS;
}

void readwords(struct GameInfo* gameInfo)
{
	FILE* fd;
	int i = 0;
	size_t len = 0;

	if ((fd = fopen("words", "r")) == NULL)
		error("Cannot open 'words' file");

	while (getline(&(gameInfo->allWords[i++]), &len, fd) != -1);

	if (fclose(fd) == EOF)
		error("Cannot close 'words' file");
}

void *game(void* arg)
{
	struct GameWorkThreadParams* gwtp = (struct GameWorkThreadParams*)arg;
	int indeks = gwtp->player;
	int fd = gwtp->gameInfo->clients[indeks]->fd;
	int i, j;
	char buf[30];

	if (snprintf(buf, 30, "Your opponent is: %d\n", gwtp->opponent) == -1)
		error("Cannot initialize buffer");

	wwrite(fd, buf, strlen(buf));

	for (i = 0; i < GAME_WORDS; ++i)
	{
		if(wwrite(fd, gwtp->words[i], strlen(gwtp->words[i])) == -1)
		{
			fprintf(stderr, "Client unconnected\n");
			safemutexlock(&(gwtp->gtp->finishGame_mutex));
			gwtp->gtp->winner = gwtp->opponent;
			safemutexunlock(&(gwtp->gtp->finishGame_mutex));
			pthread_cond_signal(&(gwtp->gtp->endGame));
			return (void*)EXIT_SUCCESS;
		}
		memset(buf, '\0', sizeof(buf));
		char* p = buf;
		if(wread(fd, &p, 30) == -1)
		{
			fprintf(stderr, "Client unconnected\n");
			safemutexlock(&(gwtp->gtp->finishGame_mutex));
			gwtp->gtp->winner = gwtp->opponent;
			safemutexunlock(&(gwtp->gtp->finishGame_mutex));
			pthread_cond_signal(&(gwtp->gtp->endGame));
			return (void*)EXIT_SUCCESS;
		}
		for(j = 0; j < strlen(gwtp->words[i]) - 1; ++j)
		{
			if (gwtp->words[i][j] != p[j])
			{
				--i;
				break;
			}
		}
	}

	safemutexlock(&(gwtp->gtp->finishGame_mutex));
	gwtp->gtp->winner = gwtp->player;
	safemutexunlock(&(gwtp->gtp->finishGame_mutex));
	pthread_cond_signal(&(gwtp->gtp->endGame));
	pthread_cond_signal(&(gwtp->gtp->gameInfo->clients[indeks]->gameEnded));

	return (void*)EXIT_SUCCESS;
}

void updateRank(struct GameInfo* gi, int winner, int looser)
{
	safemutexlock(&(gi->clients_mutex[winner]));
	gi->clients[winner]->numOfPlayedGames++;
	gi->clients[winner]->rank++;
	safemutexunlock(&(gi->clients_mutex[winner]));
	safemutexlock(&(gi->clients_mutex[looser]));
	gi->clients[looser]->numOfPlayedGames++;
	safemutexunlock(&(gi->clients_mutex[looser]));
	safemutexlock(&(gi->numOfPlayedGames_mutex));
	gi->numOfPlayedGames++;
	safemutexunlock(&(gi->numOfPlayedGames_mutex));
}

void* playgame(void* arg)
{
	pthread_t tids[2], tid;
	int i, j, looser;
	srand(time(NULL));
	int usedWords[NUM_OF_WORDS];
	struct GameThreadParams* gtp = (struct GameThreadParams*)arg;
	struct GameWorkThreadParams gwtp1, gwtp2;
	gwtp1.player = gtp->player1;
	gwtp1.opponent = gtp->player2;
	gwtp1.gameInfo = gtp->gameInfo;
	gwtp1.gtp = gtp;
	gwtp2.player = gtp->player2;
	gwtp2.opponent = gtp->player1;
	gwtp2.gameInfo = gtp->gameInfo;
	gwtp2.gtp = gtp;

	for (i = 0; i < NUM_OF_WORDS; ++i)
		usedWords[i] = 0;

	i = 0;
	while(1)
	{
		j = rand()%NUM_OF_WORDS;
		if (usedWords[j] == 1)
			continue;
		gwtp1.words[i] = gtp->gameInfo->allWords[j];
		gwtp2.words[i] = gtp->gameInfo->allWords[j];
		usedWords[j] = 1;
		if (i == 4)
			break;
		++i;
	}

	safemutexlock(&(gtp->gameInfo->clients[gtp->player1]->playingGame));
	safemutexlock(&(gtp->gameInfo->clients[gtp->player2]->playingGame));
	/*struct timespec *wait;
	wait = (struct timespec*)malloc(sizeof(struct timespec));
	wait->tv_sec = 1;
	wait->tv_nsec = 0;
	int busy = 0;
	if (pthread_mutex_timedlock(&(gtp->gameInfo->clients[gtp->player2]->playingGame), wait))
	{
		if (errno != ETIMEDOUT)
			error("Cannot lock mutex");
		busy = 1;
	}
	if (busy == 1)
	{
		pthread_cond_wait(&(gtp->gameInfo->clients[gtp->player2]->gameEnded), &(gtp->gameInfo->clients[gtp->player1]->playingGame));
		safemutexlock(&(gtp->gameInfo->clients[gtp->player2]->playingGame));
	}*/

	pthread_create(&(tids[0]), NULL, game, &gwtp1);
	pthread_create(&(tids[1]), NULL, game, &gwtp2);

	safemutexlock(&(gtp->finishGame_mutex));
	//while(gtp->winner == 0)
		pthread_cond_wait(&(gtp->endGame), &(gtp->finishGame_mutex));
	tid = gtp->winner == gtp->player1 ? tids[1] : tids[0];
	pthread_cancel(tid);
	looser = gtp->winner == gtp->player1 ? gtp->player2 : gtp->player1;
	wwrite(looser, "You lost\n", 10);
	wwrite(gtp->winner, "You won!\n", 10);
	safemutexunlock(&(gtp->finishGame_mutex));
	updateRank(gtp->gameInfo, gtp->winner, looser);

	pthread_join(tids[0], NULL);
	pthread_join(tids[1], NULL);

	safemutexunlock(&(gtp->gameInfo->clients[gtp->player1]->playingGame));
	safemutexunlock(&(gtp->gameInfo->clients[gtp->player2]->playingGame));

	pthread_cond_signal(&(gtp->gameInfo->finishedGame));
	fprintf(stderr, "sent finishedGame signal\n");

	return (void*)EXIT_SUCCESS;
}

void sendranking(struct GameInfo* gi)
{
	int rank[MAX_CLIENT];
	int ranking[MAX_CLIENT];
	int i, j = 0;
	int maxRank = 0;
	int maxIndex = 0;
	for (i = 0; i < MAX_CLIENT; ++i)
		rank[i] = -1;
	ranking[0] = -1;

	while (j < gi->numOfClients)
	{
		maxRank = -1;
		for (i = 0; i < MAX_CLIENT; ++i)
		{
			safemutexlock(&(gi->clients_mutex[i]));
			if (gi->clients[i]->fd == 0)
			{
				safemutexunlock(&(gi->clients_mutex[i]));
				continue;
			}
			if (gi->clients[i]->rank > maxRank && rank[i] == -1)
			{
				maxRank = gi->clients[i]->rank;
				maxIndex = i;
			}
			safemutexunlock(&(gi->clients_mutex[i]));
		}
		rank[maxIndex] = maxRank;
		ranking[j++] = maxIndex;
	}

	if (ranking[0] == -1)
		return;

	char *buf;
	if ((buf = (char*)malloc(50 * MAX_CLIENT * sizeof(char))) == NULL)
		error("Cannot allocate memory");

	char message[50];
	for (i = 0; i < j; ++i)
	{
		if (snprintf(message, 50, "%d. nick: %d - rank: %d\n", i, ranking[i], rank[ranking[i]]) == -1)
			error("Cannot write to buffer");
		buf = strcat(buf, message);
	}

	for (i = 0; i < MAX_CLIENT; ++i)
	{
		safemutexlock(&(gi->clients_mutex[i]));
		if (gi->clients[i]->fd != 0)
			wwrite(gi->clients[i]->fd, buf, 50*MAX_CLIENT);
		safemutexunlock(&(gi->clients_mutex[i]));
	}
	free(buf);
}

void* showranking(void* arg)
{
	struct GameInfo* gi = (struct GameInfo*)arg;
	fprintf(stderr, "Inside showranking\n");
	while (1)
	{
		safemutexlock(&(gi->finishedGame_mutex));
		fprintf(stderr, "lock finishedGame_mutex\n");
		pthread_cond_wait(&(gi->finishedGame), &(gi->finishedGame_mutex));
		fprintf(stderr, "got finishedGame signal!\n");
		safemutexlock(&(gi->numOfGames_mutex));
		fprintf(stderr, "Num of games = %d, num of played games = %d\n", gi->numOfGames, gi->numOfPlayedGames);
		if (gi->numOfGames != 0 && gi->numOfGames == gi->numOfPlayedGames)
			sendranking(gi);
		safemutexunlock(&(gi->numOfGames_mutex));
		safemutexunlock(&(gi->finishedGame_mutex));
	}

	return (void*)EXIT_SUCCESS;
}

void* gamework(void* args)
{
	struct GameInfo* gameInfo = (struct GameInfo*)args;
	int i, j;
	pthread_t tid;
	readwords(gameInfo);
	pthread_create(&tid, NULL, showranking, &gameInfo);
	while (work)
	{
		//safemutexlock(&(gameInfo->changeInClient_mutex));
		//pthread_cond_wait(&(gameInfo->changeInClient), &(gameInfo->changeInClient_mutex));
		for (i = 0; i < MAX_CLIENT; ++i)
		{
			for (j = 0; j < MAX_CLIENT; ++j)
			{
				if (i == j)
					continue;
				if (gameInfo->clients[i]->fd == 0 || gameInfo->clients[j]->fd == 0)
					continue;
				safemutexlock(&(gameInfo->games_mutex));
				if (gameInfo->games[i][j] == 0)
				{
					gameInfo->games[i][j] = 1;
					gameInfo->games[j][i] = 1;
					struct GameThreadParams gtp;
					pthread_mutex_init(&(gtp.finishGame_mutex), NULL);
					pthread_cond_init(&(gtp.endGame),NULL);
					gtp.gameInfo = gameInfo;
					gtp.player1 = i;
					gtp.player2 = j;
					gtp.winner = 0;
					pthread_create(&(gameInfo->tids[i][j]), NULL, playgame, &gtp);
				}
				safemutexunlock(&(gameInfo->games_mutex));
			}
		}
		//safemutexunlock(&(gameInfo->changeInClient_mutex));
	}

	for (i = 0; i < MAX_CLIENT; ++i)
		for (j = 0; j < MAX_CLIENT; ++j)
			if (gameInfo->tids[i][j] != 0)
				pthread_join(gameInfo->tids[i][j], NULL);
	pthread_join(tid, NULL);

	return (void*)EXIT_SUCCESS;
}

void initilizecomponents(struct GameInfo* gameInfo)
{
	int i;
	if ((gameInfo->clients = (struct Client**)calloc(MAX_CLIENT, sizeof(struct Client*))) == NULL)
		error("Cannot allocate memory for Client array");
	if ((gameInfo->games = (int**)calloc(MAX_CLIENT, sizeof(int*))) == NULL)
		error("Cannot allocate memory of games array");
	if ((gameInfo->tids = (pthread_t**)calloc(MAX_CLIENT, sizeof(pthread_t*))) == NULL)
		error("Cannot allocate memory of tids array");
	for (i = 0; i < MAX_CLIENT; ++i)
	{
		pthread_mutex_init(&(gameInfo->clients_mutex[i]), NULL);
		if ((gameInfo->clients[i] = (struct Client*)calloc(1, sizeof(struct Client))) == NULL)
			error("Cannot allocate memory for Client array");
		if ((gameInfo->games[i] = (int*)calloc(1, sizeof(int))) == NULL)
			error("Cannot allocate memory for games array");
		if ((gameInfo->tids[i] = (pthread_t*)calloc(1, sizeof(pthread_t))) == NULL)
			error("Cannot allocate memory for tids array");
		pthread_mutex_init(&(gameInfo->clients[i]->playingGame), NULL);
		pthread_cond_init(&(gameInfo->clients[i]->gameEnded),NULL);
		//gameInfo->clients[i] = NULL;
		//gameInfo->clients[i]->fd = 0;
	}
	gameInfo->numOfClients = 0;
	gameInfo->numOfGames = 0;
	gameInfo->numOfPlayedGames = 0;
	pthread_mutex_init(&(gameInfo->numOfClients_mutex), NULL);
	pthread_mutex_init(&(gameInfo->numOfGames_mutex), NULL);
	pthread_mutex_init(&(gameInfo->games_mutex), NULL);
	pthread_mutex_init(&(gameInfo->numOfPlayedGames_mutex), NULL);
	pthread_mutex_init(&(gameInfo->changeInClient_mutex), NULL);
	pthread_mutex_init(&(gameInfo->finishedGame_mutex), NULL);

	pthread_cond_init(&(gameInfo->changeInClient),NULL);
	pthread_cond_init(&(gameInfo->finishedGame),NULL);
	//pthread_mutex_init(&(gameInfo.clients_mutex), NULL);
}

void deletecomponents(struct GameInfo* gameInfo)
{
	int i;
	for (i = 0; i < MAX_CLIENT; ++i)
	{
		free(gameInfo->clients[i]);
		free(gameInfo->games[i]);
		free(gameInfo->tids[i]);
	}
	free(gameInfo->clients);
	free(gameInfo->games);
	free(gameInfo->tids);
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
	pthread_t tidserver, tidgame;
	sigset_t mask;
	sigemptyset(&mask);
	sigaddset(&mask, SIGINT);

	if (argc != 2)
		USAGE(argv[0]);
	port = atoi(argv[1]);
	if (port < 1 || port > 65535)
		USAGE(argv[0]);

	initilizecomponents(&gameInfo);
	work = 1;
	registerhandlers();
	s = create_socket(port);
	fprintf(stderr, "Created Server socket\n");
	serverThreadParams.s = s;
	serverThreadParams.gameInfo = &gameInfo;
	pthread_create(&tidserver, NULL, serverwork, &serverThreadParams);
	//serverwork(s/*, &gameInfo*/);
	//if (pthread_sigmask(SIG_BLOCK, &mask, NULL))
	//	error("Main: Cannot block SIGINT");

	//playgame(&gameInfo);

	pthread_create(&tidgame, NULL, gamework, &gameInfo);
	pthread_join(tidgame, NULL);
	pthread_join(tidserver, NULL);

	if (TEMP_FAILURE_RETRY(close(s)) == -1)
		error("Cannot close socket");

	deletecomponents(&gameInfo);
	return EXIT_SUCCESS;
}



