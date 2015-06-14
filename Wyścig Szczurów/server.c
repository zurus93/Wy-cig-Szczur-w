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
	int fd, indeks;
	char buf[100];

	if ((fd = TEMP_FAILURE_RETRY(accept(s, NULL, NULL))) == -1)
		error("Cannot accept connection");

	FD_SET(fd, mfds);
	*fdmax = (*fdmax < fd) ? fd : *fdmax;

	// Check if the is place for one more player
	if (gameInfo->numOfClients >= MAX_CLIENT)
	{
		if(snprintf(buf, 100, "We're sorry, to many log in clients. Try again later") == -1)
			error("Cannot initialize buffer");
		wwrite(fd, buf, strlen(buf));
		fflush(NULL);
		deleteclient(fd, mfds, gameInfo);
		return 0;
	}
	else
		gameInfo->numOfClients++;

	// Update new number of games to be played
	gameInfo->numOfGames = factorial(gameInfo->numOfClients);

	indeks = fd%MAX_CLIENT;
	if (gameInfo->clients[indeks] != NULL)
		while ((indeks = (indeks+1)%MAX_CLIENT) != fd%MAX_CLIENT)
			if (gameInfo->clients[indeks] == NULL)
				break;

	safemutexlock(&(gameInfo->clients[indeks]->client_mutex));
	gameInfo->clients[indeks]->fd = fd;
	gameInfo->clients[indeks]->indeks = indeks;
	gameInfo->clients[indeks]->rank = 0;
	gameInfo->clients[indeks]->playingGame = 0;
	safemutexunlock(&(gameInfo->clients[indeks]->client_mutex));

	if (snprintf(buf, 20, "Your nick is: %d\n", fd) == -1)
		error("Cannot initialize buffer");

	wwrite(fd, buf, strlen(buf));
	fflush(NULL);
	pthread_cond_signal(&(gameInfo->changeInClient));

	return 1;
}

int deleteclient(int s, fd_set* mfds, struct GameInfo* gameInfo)
{
	int i, indeks, gamescount = 0;
	if (TEMP_FAILURE_RETRY(close(s)) == -1)
		error("Cannot close socket");
	FD_CLR(s, mfds);

	indeks = s%MAX_CLIENT;

	if (gameInfo->clients[indeks] == NULL || (gameInfo->clients[indeks]->fd)%MAX_CLIENT != gameInfo->clients[indeks]->indeks)
		while ((indeks = (indeks+1)%MAX_CLIENT) != s%MAX_CLIENT)
			if (gameInfo->clients[indeks] != NULL && gameInfo->clients[indeks]->fd == s)


	safemutexlock(&(gameInfo->clients[indeks]->client_mutex));
	gameInfo->clients[indeks]->fd = 0;
	gameInfo->clients[indeks]->indeks = 0;
	gameInfo->clients[indeks]->rank = 0;
	gameInfo->clients[indeks]->playingGame = 0;
	safemutexunlock(&(gameInfo->clients[indeks]->client_mutex));

	// Update new number of games to be played
	gameInfo->numOfClients--;
	gameInfo->numOfGames = factorial(gameInfo->numOfClients);

	safemutexlock(&(gameInfo->games_mutex));
	for (i = 0; i < MAX_CLIENT; ++i)
	{
		if (gameInfo->games[indeks][i] == 1)
			++gamescount;
		gameInfo->games[indeks][i] = 0;
		gameInfo->games[i][indeks] = 0;
	}
	safemutexunlock(&(gameInfo->games_mutex));

	safemutexlock(&(gameInfo->numOfPlayedGames_mutex));
	gameInfo->numOfPlayedGames -= gamescount;
	safemutexunlock(&(gameInfo->numOfPlayedGames_mutex));

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

	while (1)
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

void selectwinner (pthread_mutex_t* finishGame_mutex, int* winner, int w, pthread_cond_t* endGame)
{
	fflush(NULL);
	safemutexlock(finishGame_mutex);
	*winner = w;
	pthread_cond_signal(endGame);
	safemutexunlock(finishGame_mutex);
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

	if (wwrite(fd, buf, strlen(buf)) == -1)
	{
		selectwinner(&(gwtp->gtp->finishGame_mutex), &(gwtp->gtp->winner), gwtp->opponent, &(gwtp->gtp->endGame));
		return (void*)EXIT_SUCCESS;
	}
	fflush(NULL);

	for (i = 0; i < GAME_WORDS; ++i)
	{
		if(wwrite(fd, gwtp->words[i], strlen(gwtp->words[i])) == -1)
		{
			selectwinner(&(gwtp->gtp->finishGame_mutex), &(gwtp->gtp->winner), gwtp->opponent, &(gwtp->gtp->endGame));
			return (void*)EXIT_SUCCESS;
		}
		fflush(NULL);
		memset(buf, '\0', sizeof(buf));
		char* p = buf;
		if(wread(fd, &p, 30) == -1)
		{
			selectwinner(&(gwtp->gtp->finishGame_mutex), &(gwtp->gtp->winner), gwtp->opponent, &(gwtp->gtp->endGame));
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

	selectwinner(&(gwtp->gtp->finishGame_mutex), &(gwtp->gtp->winner), gwtp->player, &(gwtp->gtp->endGame));

	return (void*)EXIT_SUCCESS;
}

void updateRank(struct GameInfo* gi, int winner, int looser)
{
	safemutexlock(&(gi->clients[winner]->client_mutex));
	gi->clients[winner]->rank++;
	safemutexunlock(&(gi->clients[winner]->client_mutex));

	safemutexlock(&(gi->numOfPlayedGames_mutex));
	gi->numOfPlayedGames++;
	safemutexunlock(&(gi->numOfPlayedGames_mutex));
}

void* playgame(void* arg)
{
	struct GameThreadParams* gtp = (struct GameThreadParams*)arg;
	fflush(NULL);

	// Mark that the clients are currently playing
	safemutexlock(&(gtp->gameInfo->clients[gtp->player1]->client_mutex));
	gtp->gameInfo->clients[gtp->player1]->playingGame = 1;
	safemutexunlock(&(gtp->gameInfo->clients[gtp->player1]->client_mutex));
	safemutexlock(&(gtp->gameInfo->clients[gtp->player2]->client_mutex));
	gtp->gameInfo->clients[gtp->player2]->playingGame = 1;
	safemutexunlock(&(gtp->gameInfo->clients[gtp->player2]->client_mutex));

	pthread_t tids[2], tid;
	int i, j, looser;
	srand(time(NULL));
	int usedWords[NUM_OF_WORDS];

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

	pthread_create(&(tids[0]), NULL, game, &gwtp1);
	pthread_create(&(tids[1]), NULL, game, &gwtp2);

	safemutexlock(&(gtp->finishGame_mutex));
	pthread_cond_wait(&(gtp->endGame), &(gtp->finishGame_mutex));

	// Select player whose lost and send him cancel command
	tid = gtp->winner == gtp->player1 ? tids[1] : tids[0];
	pthread_cancel(tid);
	looser = gtp->winner == gtp->player1 ? gtp->player2 : gtp->player1;
	if (gtp->gameInfo->clients[looser]->fd > 3)
		wwrite(gtp->gameInfo->clients[looser]->fd, "You lost\n", 10);
	fflush(NULL);
	if (gtp->gameInfo->clients[gtp->winner]->fd > 3)
		wwrite(gtp->gameInfo->clients[gtp->winner]->fd, "You won!\n", 10);
	fflush(NULL);
	safemutexunlock(&(gtp->finishGame_mutex));

	updateRank(gtp->gameInfo, gtp->winner, looser);

	safemutexlock(&(gtp->gameInfo->clients[gtp->player1]->client_mutex));
	gtp->gameInfo->clients[gtp->player1]->playingGame = 0;
	safemutexunlock(&(gtp->gameInfo->clients[gtp->player1]->client_mutex));
	safemutexlock(&(gtp->gameInfo->clients[gtp->player2]->client_mutex));
	gtp->gameInfo->clients[gtp->player2]->playingGame = 0;
	safemutexunlock(&(gtp->gameInfo->clients[gtp->player2]->client_mutex));

	free(gtp);

	return (void*)EXIT_SUCCESS;
}

void sendranking(struct GameInfo* gi)
{
	if (gi->numOfClients < 2) return;
	int i;
	char *buf;
	if ((buf = (char*)malloc(50 * MAX_CLIENT * sizeof(char))) == NULL)
		error("Cannot allocate memory");
	memset(buf, '\0', sizeof(buf));

	char message[50];
	int len = 0;
	int j = 1;
	for (i = 0; i < MAX_CLIENT; ++i)
	{
		safemutexlock(&(gi->clients[i]->client_mutex));
		if (gi->clients[i] != NULL && gi->clients[i]->fd > 3)
		{
			if (snprintf(message, 50, "%d. nick: %d - rank: %d\n", j, gi->clients[i]->fd, gi->clients[i]->rank) == -1)
				error("Cannot write to buffer");
			len += strlen(message);
			buf = strcat(buf, message);
			++j;
		}
		safemutexunlock(&(gi->clients[i]->client_mutex));
	}

	for (i = 0; i < MAX_CLIENT; ++i)
	{
		if (gi->clients[i]->fd > 3)
		{
			wwrite(gi->clients[i]->fd, buf, len);
			fflush(NULL);
		}
	}

	free(buf);
}

void* gamework(void* args)
{
	struct GameInfo* gameInfo = (struct GameInfo*)args;
	int i, j;
	readwords(gameInfo);
	struct GameThreadParams* gtp[MAX_CLIENT][MAX_CLIENT];

	while (work)
	{
		safemutexlock(&(gameInfo->changeInClient_mutex));
		pthread_cond_wait(&(gameInfo->changeInClient), &(gameInfo->changeInClient_mutex));
		while (gameInfo->numOfPlayedGames < gameInfo->numOfGames)
		{
			for (i = 0; i < MAX_CLIENT; ++i)
			{
				for (j = 0; j < MAX_CLIENT; ++j)
				{
					if (i == j)
						continue;
					if (gameInfo->clients[i]->fd <= 3 || gameInfo->clients[j]->fd <= 3)
						continue;
					if (gameInfo->clients[i]->playingGame == 1 || gameInfo->clients[j]->playingGame == 1)
						continue;

					safemutexlock(&(gameInfo->games_mutex));
					if (gameInfo->games[i][j] == 0)
					{
						gameInfo->games[i][j] = 1;
						gameInfo->games[j][i] = 1;
						if ((gtp[i][j] = (struct GameThreadParams*)malloc(sizeof(struct GameThreadParams))) == NULL)
							error("Cannot allocate memory");
						pthread_mutex_init(&(gtp[i][j]->finishGame_mutex), NULL);
						pthread_cond_init(&(gtp[i][j]->endGame),NULL);
						gtp[i][j]->gameInfo = gameInfo;
						gtp[i][j]->player1 = i;
						gtp[i][j]->player2 = j;
						gtp[i][j]->winner = 0;
						pthread_create(&(gameInfo->tids[i][j]), NULL, playgame, gtp[i][j]);
					}
					safemutexunlock(&(gameInfo->games_mutex));
				}
			}
			sleep(1);
		}
		fflush(NULL);
		sendranking(gameInfo);
		safemutexunlock(&(gameInfo->changeInClient_mutex));
	}

	for (i = 0; i < MAX_CLIENT; ++i)
		for (j = 0; j < MAX_CLIENT; ++j)
			if (gameInfo->tids[i][j] != 0)
				pthread_join(gameInfo->tids[i][j], NULL);

	return (void*)EXIT_SUCCESS;
}

void initilizecomponents(struct GameInfo* gameInfo)
{
	int i;
	for (i = 0; i < MAX_CLIENT; ++i)
	{
		if ((gameInfo->clients[i] = (struct Client*)calloc(1, sizeof(struct Client))) == NULL)
			error("Cannot allocate memory for Client array");
		pthread_mutex_init(&(gameInfo->clients[i]->client_mutex), NULL);
	}
	gameInfo->numOfClients = 0;
	gameInfo->numOfGames = 0;
	gameInfo->numOfPlayedGames = 0;
	pthread_mutex_init(&(gameInfo->games_mutex), NULL);
	pthread_mutex_init(&(gameInfo->numOfPlayedGames_mutex), NULL);
	pthread_mutex_init(&(gameInfo->changeInClient_mutex), NULL);

	pthread_cond_init(&(gameInfo->changeInClient),NULL);

	for (i = 0; i < MAX_CLIENT; ++i)
	{
		gameInfo->clients[i]->fd = 0;
		gameInfo->clients[i]->rank = 0;
	}
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

	struct GameInfo* gameInfo;
	if ((gameInfo = (struct GameInfo*)malloc(sizeof(struct GameInfo))) == NULL)
		error("Cannot allocate gameInfo buffer");

	initilizecomponents(gameInfo);
	work = 1;
	registerhandlers();
	s = create_socket(port);
	fprintf(stderr, "Created Server socket\n");
	serverThreadParams.s = s;
	serverThreadParams.gameInfo = gameInfo;
	pthread_create(&tidserver, NULL, serverwork, &serverThreadParams);

	pthread_create(&tidgame, NULL, gamework, gameInfo);
	pthread_join(tidgame, NULL);
	pthread_join(tidserver, NULL);

	if (TEMP_FAILURE_RETRY(close(s)) == -1)
		error("Cannot close socket");

	deletecomponents(gameInfo);
	return EXIT_SUCCESS;
}



