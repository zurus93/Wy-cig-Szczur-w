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
	struct Client** clients;
}gameInfo;

#endif /* CLIENT_H_ */
