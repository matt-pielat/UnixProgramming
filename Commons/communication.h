/*
 * communication.h
 *
 *  Created on: Jun 6, 2015
 *      Author: matthew
 */

#ifndef COMMUNICATION_H_
#define COMMUNICATION_H_

#include "libraries.h"
#include "files.h"
#include "utilities.h"

#define DGRAMSIZE 576
#define DATACHUNKSIZE 512 //Check with dataChunkMessage

#define TIMEOUT_SHORT 5
#define TIMEOUT_MEDIUM 10
#define TIMEOUT_LONG 30

#define REQUEST_HANDSHAKE 'h'
#define REQUEST_UPLOAD 'u'
#define REQUEST_DOWNLOAD 'd'
#define REQUEST_DELETE 'r'
#define REQUEST_LIST 'l'

#define MSGTYPE_REQUEST		1
#define MSGTYPE_RESPONSE	2
#define MSGTYPE_METADATA	3
#define MSGTYPE_DATA		4

#define ERROR_NOERROR		0
#define ERROR_TIMEOUT		1
#define ERROR_UPLOADING		2
#define ERROR_DOWNLOADING	3
#define ERROR_NOTFOUND		4
#define ERROR_TOOMUCHDATA	5

typedef struct{
	char msgType;
	char request;
	off_t byteOffset;
	char filename[MAXFILENAMESIZE];
} requestMessage;

typedef struct{
	char msgType;
	char error;
} responseMessage;

typedef struct{
	char msgType;
	size_t totalSize;
	md5hash checksum; //Whole file checksum (no matter the offset).
	char filename[MAXFILENAMESIZE];
} metadataMessage;

typedef struct{
	char msgType;
	size_t size; //Equals DATACHUNKSIZE most of the time.
	char data[DATACHUNKSIZE];
} dataMessage;

int makeUdpSocket(uint16_t port, int timeout);
void closeSocket(int sock);
void sendDatagram(int sock, struct sockaddr_in addr, void* buf);
void sendBroadcastDatagram(int sock, uint16_t destPort, void* buf);
int receiveDatagram(int sock, struct sockaddr_in* addr, void* buf);
size_t sendData(int sock, struct sockaddr_in addr, void *buf, size_t len);
size_t receiveData(int sock, void *buf, size_t len);
size_t sendFile(int sock, struct sockaddr_in addr, char *filepath, off_t offset);
size_t receiveFile(int sock, char *filepath);

#endif /* COMMUNICATION_H_ */
