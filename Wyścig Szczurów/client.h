/*
 * client.h
 * Structures for use in server
 *
 *  Created on: 17 maj 2015
 *      Author: Monika Żurkowska
 */

#ifndef CLIENT_H_
#define CLIENT_H_

#define MAX_CLIENT 1000
#define NUM_OF_WORDS 22
#define GAME_WORDS 5

struct Client
{
	int fd;
	int indeks;
	int rank;
	int numOfPlayedGames;
	pthread_mutex_t playingGame;
	pthread_cond_t gameEnded;
};

struct GameInfo
{
	pthread_mutex_t clients_mutex[MAX_CLIENT];
	pthread_mutex_t numOfClients_mutex;
	pthread_mutex_t numOfGames_mutex;
	pthread_mutex_t numOfPlayedGames_mutex;
	pthread_mutex_t games_mutex;
	pthread_mutex_t changeInClient_mutex;
	pthread_mutex_t finishedGame_mutex;;
	pthread_cond_t changeInClient;
	pthread_cond_t finishedGame;
	pthread_t** tids;
	struct Client** clients;
	int numOfClients;
	int** games;
	int numOfGames;
	int numOfPlayedGames;
	char* allWords[NUM_OF_WORDS];

}gameInfo;

struct ServerThreadParams
{
	int s;
	struct GameInfo* gameInfo;
}serverThreadParams;

struct GameThreadParams
{
	struct GameInfo* gameInfo;
	pthread_cond_t endGame;
	pthread_mutex_t finishGame_mutex;
	int winner;
	int player1;
	int player2;
};

struct GameWorkThreadParams
{
	struct GameInfo* gameInfo;
	struct GameThreadParams* gtp;
	int player;
	int opponent;
	char* words[GAME_WORDS];
};

#endif /* CLIENT_H_ */
