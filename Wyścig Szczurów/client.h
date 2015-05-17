/*
 * client.h
 * Class representing connected Client
 *  Created on: 17 maj 2015
 *      Author: monika
 */

#ifndef CLIENT_H_
#define CLIENT_H_

struct Client
{
	int fd;
	int rank;
	char nick[10];
};

#endif /* CLIENT_H_ */
