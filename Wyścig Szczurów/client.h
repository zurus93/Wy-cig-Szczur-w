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

struct Client
{
	int fd;
	int index;
	int rank;
	int numOfPlayedGames;
};

struct GameInfo
{
	pthread_mutex_t clients_mutex[MAX_CLIENT];
	pthread_mutex_t numOfClients_mutex;
	struct Client** clients;
	int numOfClients;
	int** games;
}gameInfo;

struct ServerThreadParams
{
	int s;
	struct GameInfo* gameInfo;
}serverThreadParams;

#endif /* CLIENT_H_ */
