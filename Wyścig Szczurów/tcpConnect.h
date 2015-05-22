/*
 * tcpConnect.h
 * Class responsible for maintaning tcp connection
 *
 *  Created on: 17 maj 2015
 *      Author: Monika Żurkowska
 */

#ifndef TCPCONNECT_H_
#define TCPCONNECT_H_

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <pthread.h>

volatile sig_atomic_t work;

void error(char*);
void siginthandler(int);
void registerhandlers(void);
ssize_t wread(int, char**, size_t);
ssize_t wwrite(int, const char*, size_t);
void safemutexlock(pthread_mutex_t* mutex);
void safemutexunlock(pthread_mutex_t* mutex);

#endif /* TCPCONNECT_H_ */
