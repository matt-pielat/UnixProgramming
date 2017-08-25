/*
 * requests.h
 *
 *  Created on: Jun 13, 2015
 *      Author: matthew
 */

#ifndef REQUESTS_H_
#define REQUESTS_H_

#include "files.h"

typedef struct{
	struct sockaddr_in addr;
	char dirpath[MAXDIRPATHSIZE];
} listRequestArg;

typedef struct{
	struct sockaddr_in addr;
	char dirpath[MAXDIRPATHSIZE];
	char filename[MAXFILENAMESIZE];
	size_t byteOffset;
} downloadRequestArg;

typedef struct{
	struct sockaddr_in addr;
	char dirpath[MAXDIRPATHSIZE];
	char filename[MAXFILENAMESIZE];
	size_t byteOffset;
} uploadRequestArg;

typedef struct{
	struct sockaddr_in addr;
	char dirpath[MAXDIRPATHSIZE];
	char filename[MAXFILENAMESIZE];
} removeRequestArg;

//----------------------------------------------------------

void handleHandshakeRequest(int sock, struct sockaddr_in addr);
void* handleListRequest(void* arg);
void* handleDownloadRequest(void* arg);
void* handleUploadRequest(void* arg);
void* handleRemoveRequest(void* arg);
void* onExit(int sock);

#endif /* REQUESTS_H_ */
