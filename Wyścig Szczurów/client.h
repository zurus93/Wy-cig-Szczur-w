/*
 * client.h
 * Structures for use in server
 *
 *  Created on: 17 maj 2015
 *      Author: Monika Żurkowska
 */

#ifndef CLIENT_H_
#define CLIENT_H_

struct Client
{
	int fd;
	int rank;
	char nick[10];
};

struct GameInfo
{
	pthread_mutex_t clients_mutex;
	struct Client** clients;
}gameInfo;

struct ServerThreadParams
{
	int s;
	struct GameInfo* gameInfo;
}serverThreadParams;

#endif /* CLIENT_H_ */
