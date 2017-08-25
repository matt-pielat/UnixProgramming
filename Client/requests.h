/*
 * commands.h
 *
 *  Created on: Jun 13, 2015
 *      Author: matthew
 */

#ifndef COMMANDS_H_
#define COMMANDS_H_

//----------------------------------------------------------

#include "libraries.h"

#define CMD_LIST		'l'
#define CMD_DOWNLOAD	'd'
#define CMD_UPLOAD		'u'
#define CMD_DELETE		'r'
#define CMD_QUIT		'q'

typedef struct{
	struct sockaddr_in serverAddr;
} listRequestArg;

typedef struct{
	struct sockaddr_in serverAddr;
	char dirpath[MAXDIRPATHSIZE];
	char filename[MAXFILENAMESIZE];
} downloadRequestArg;

typedef struct{
	struct sockaddr_in serverAddr;
	char dirpath[MAXDIRPATHSIZE];
	char filename[MAXFILENAMESIZE];
} uploadRequestArg;

typedef struct{
	struct sockaddr_in serverAddr;
	char filename[MAXFILENAMESIZE];
} removeRequestArg;

//----------------------------------------------------------

void* listRequest(void *arg);
void* downloadRequest(void *arg);
void* uploadRequest(void *arg);
void* removeRequest(void *arg);
void* onExit(int sock, int timeout);

#endif /* COMMANDS_H_ */
