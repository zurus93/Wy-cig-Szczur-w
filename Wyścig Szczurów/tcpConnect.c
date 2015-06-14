/*
 * tcpConnect.c
 *
 *  Created on: 17 maj 2015
 *      Author: monika
 */
#define _GNU_SOURCE

#include "tcpConnect.h"

void error(char* msg)
{
	perror(msg);
	exit(EXIT_FAILURE);
}

void siginghandler(int sig)
{
	work = 0;
}

void registerhandlers(void)
{
	struct sigaction sa;
	memset (&sa, 0, sizeof(sa));
	/*sa.sa_handler = siginghandler;

	if (sigaction(SIGINT, &sa, NULL) == -1)
		error("Cannot register SIGINT");*/

	sa.sa_handler = SIG_IGN;
	if (sigaction(SIGPIPE, &sa, NULL) == -1)
		error("Cannot register SIGPIPE");
}

ssize_t wread(int fd, char** buf, size_t size)
{
	fflush(NULL);
	ssize_t nleft = size;
	ssize_t nread;
	char* p = *buf;

	while (1)
	{
		if ((nread = read(fd, p, nleft)) == -1)
		{
			if (errno == EINTR)	continue;
			else return -1;
		}
		break;
	}
	fflush(NULL);
	return size - nleft;
}

ssize_t wwrite (int fd, const char* buf, size_t size)
{
	fflush(NULL);
	size_t nleft = size;
	ssize_t nwritten;
	const char* p = buf;

	while (nleft > 0)
	{
		if ((nwritten = write(fd, p, nleft)) == -1)
		{
			if (errno == EINTR) nwritten = 0;
			else return -1;
		}

		nleft -= nwritten;
		p += nwritten;
	}
	fflush(NULL);
	return size;
}

void safemutexlock (pthread_mutex_t* mutex)
{
	if (pthread_mutex_lock(mutex))
		error("Cannot lock mutex");
}

void safemutexunlock (pthread_mutex_t* mutex)
{
	if (pthread_mutex_unlock(mutex))
		error("Cannot unlock mutex");
}

