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
	int indeks;;
	int playingGame;
	int rank;
	pthread_mutex_t client_mutex;
};

struct GameInfo
{
	pthread_mutex_t numOfPlayedGames_mutex;
	pthread_mutex_t games_mutex;
	pthread_mutex_t changeInClient_mutex;
	pthread_cond_t changeInClient;
	pthread_t tids[MAX_CLIENT][MAX_CLIENT];
	int numOfClients;
	int games[MAX_CLIENT][MAX_CLIENT];
	int numOfGames;
	int numOfPlayedGames;
	struct Client* clients[MAX_CLIENT];
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
