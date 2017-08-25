/*
 * communication.c
 *
 *  Created on: Jun 6, 2015
 *      Author: matthew
 */

#include "communication.h"
#include "metadata.h"

int makeUdpSocket(uint16_t port, int timeout){
	struct sockaddr_in name;
	int sock, t = 1;
	struct timeval tv;
	if ((sock = socket(PF_INET, SOCK_DGRAM, 0)) < 0){
		ERR("socket");
	}
	name.sin_family = AF_INET;
	name.sin_port = htons(port);
	name.sin_addr.s_addr = htonl(INADDR_ANY);
	if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &t, sizeof(t))){
		ERR("setsockopt");
	}
	if (bind(sock, (struct sockaddr*)&name, sizeof(name)) < 0){
		ERR("bind");
	}
	if (timeout >= 0){
		tv.tv_sec = timeout;
		tv.tv_usec = 0;
		if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0){
			ERR("setsockopt");
		}
	}
	return sock;
}

void closeSocket(int sock){
	if(TEMP_FAILURE_RETRY(close(sock)) < 0){
		ERR("close");
	}
}

void sendDatagram(int sock, struct sockaddr_in addr, void *buf){
	int status;
	struct sockaddr *a = (struct sockaddr*)&addr;
	status = TEMP_FAILURE_RETRY(sendto(sock, buf, DGRAMSIZE, 0, a, sizeof(addr)));
	if (status < 0){
		ERR("sendto");
	}
}

void sendBroadcastDatagram(int sock, uint16_t destPort, void* buf){
	struct sockaddr_in addr;
	addr.sin_addr.s_addr = htonl(-1); // 255.255.255.255
	addr.sin_port = htons(destPort);
	addr.sin_family = AF_INET;
	sendDatagram(sock, addr, buf);
}

//Returns -1 only if timeout is set and occurs.
int receiveDatagram(int sock, struct sockaddr_in *addr, void *buf){
	socklen_t l = sizeof(struct sockaddr_in);
	struct sockaddr *a = (struct sockaddr*)addr;
	int status;
	status = TEMP_FAILURE_RETRY(recvfrom(sock, buf, DGRAMSIZE, 0, a, &l));
	if (status < 0 && errno != EAGAIN){
		ERR("recvfrom");
	}
	return status;
}

size_t sendData(int sock, struct sockaddr_in addr, void *buf, size_t len){
	size_t bytesSent = 0, bytesToSend = len;

	while (bytesToSend > 0){
		char recvBuf[DGRAMSIZE];
		dataMessage dataMsg;
		responseMessage *responseMsg;

		//Send data message.
		dataMsg.msgType = MSGTYPE_DATA;
		if (bytesToSend < DATACHUNKSIZE){
			dataMsg.size = bytesToSend;
		}
		else{
			dataMsg.size = DATACHUNKSIZE;
		}
		memcpy(dataMsg.data, buf + bytesSent, dataMsg.size);
		sendDatagram(sock, addr, &dataMsg);

		//Receive response.
		if (receiveDatagram(sock, NULL, recvBuf) < 0){
			fprintf(stderr, "(sendData) Timeout.\n");
			break;
		}
		responseMsg = (responseMessage*)recvBuf;
		if (responseMsg->msgType != MSGTYPE_RESPONSE){
			ERR("responseMsg->msgType");
		}
		if (responseMsg->error != ERROR_NOERROR){
			fprintf(stderr, "(sendData) Error %d received.\n", responseMsg->error);
			break;
		}

		bytesSent += dataMsg.size;
		bytesToSend -= dataMsg.size;
	}
	return bytesSent;
}

size_t receiveData(int sock, void *buf, size_t len){
	size_t bytesReceived = 0, bytesToReceive = len;
	struct sockaddr_in addr;

	while (bytesToReceive > 0){
		char recvBuf[DGRAMSIZE];
		responseMessage responseMsg;
		dataMessage *dataMsg;

		//Receive data message.
		if (receiveDatagram(sock, &addr, recvBuf) < 0){
			fprintf(stderr, "(receiveData) Timeout.\n");
			break;
		}
		dataMsg = (dataMessage*)recvBuf;
		if (dataMsg->msgType != MSGTYPE_DATA){
			ERR("dataMsg->msgType");
		}

		//Create response and check for errors.
		responseMsg.msgType = MSGTYPE_RESPONSE;
		responseMsg.error = ERROR_NOERROR;
		if (dataMsg->size > bytesToReceive){
			responseMsg.error = ERROR_TOOMUCHDATA;
			sendDatagram(sock, addr, &responseMsg);
			break;
		}
		sendDatagram(sock, addr, &responseMsg);

		memcpy(buf + bytesReceived, dataMsg->data, dataMsg->size);
		bytesReceived += dataMsg->size;
		bytesToReceive -= dataMsg->size;
	}
	return bytesReceived;
}

//Returns the number of unsent bytes.
size_t sendFile(int sock, struct sockaddr_in addr, char *filepath, off_t offset){
	int fd;
	size_t bytesLeft;
	fileStatus fStatus;

	fd = openFile(filepath, O_RDONLY);
	bytesLeft = getFileSize(filepath) - offset;
	getFileStatus(basename(filepath), &fStatus);

	while (bytesLeft > 0){
		char readBuf[DATACHUNKSIZE];
		size_t chunkSize;

		if (bytesLeft < DATACHUNKSIZE){
			chunkSize = bytesLeft;
		}
		else{
			chunkSize = DATACHUNKSIZE;
		}
		bulkPread(fd, readBuf, chunkSize, offset);

		if (sendData(sock, addr, readBuf, chunkSize) < chunkSize){
			break;
		}

		bytesLeft -= chunkSize;
		offset += chunkSize;
		fStatus.bytesLeft = bytesLeft;
		setFileStatus(&fStatus);

		//Transfer speed constraint.
		xSleep(1);
	}

	closeFile(fd);
	return bytesLeft;
}

//Returns the number of bytes left to receive.
size_t receiveFile(int sock, char *filepath){
	int fd;
	fileStatus fStatus;
	size_t bytesLeft;

	fd = openFile(filepath, O_WRONLY|O_CREAT|O_APPEND);
	getFileStatus(basename(filepath), &fStatus);
	bytesLeft = fStatus.bytesLeft;

	while (bytesLeft > 0){
		char recvBuf[DATACHUNKSIZE];
		size_t bytesExpected;
		size_t bytesReceived;

		if (bytesLeft < DATACHUNKSIZE){
			bytesExpected = bytesLeft;
		}
		else{
			bytesExpected = DATACHUNKSIZE;
		}

		bytesReceived = receiveData(sock, recvBuf, bytesExpected);
		if (bytesExpected != bytesReceived){
			break;
		}

		bulkWrite(fd, recvBuf, bytesReceived);

		bytesLeft -= bytesReceived;
		fStatus.bytesLeft = bytesLeft;
		setFileStatus(&fStatus);

		//Transfer speed constraint.
		xSleep(1);
	}

	closeFile(fd);
	return bytesLeft;
}
