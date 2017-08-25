/*
 * requests.c
 *
 *  Created on: Jun 13, 2015
 *      Author: matthew
 */

#include "communication.h"
#include "libraries.h"
#include "metadata.h"
#include "requests.h"

void handleHandshakeRequest(int sock, struct sockaddr_in addr){
	responseMessage handshakeMsg;
	handshakeMsg.msgType = MSGTYPE_RESPONSE;
	handshakeMsg.error = ERROR_NOERROR;
	sendDatagram(sock, addr, &handshakeMsg);
}

void* handleListRequest(void *arg){
	int sock, i, n;
	char *fileList, buffer[DGRAMSIZE];
	DIR *dir;
	listRequestArg lArg;
	metadataMessage metadataMsg;
	responseMessage *responseMsg;
	struct dirent* dirEntry;

	sock = makeUdpSocket(0, TIMEOUT_SHORT);
	memcpy(&lArg, arg, sizeof(listRequestArg));
	free(arg);

	//Allocate memory for n files (reallocation may occur later).
	n = 32;
	if ((fileList = (char*)malloc(n * sizeof(fileStatus))) == NULL){
		ERR("malloc");
	}

	//Traverse the directory.
	if ((dir = opendir(lArg.dirpath)) == NULL){
		ERR("opendir");
	}
	i = 0;
	for (errno = 0; (dirEntry = readdir(dir)) != NULL; errno = 0){
		char filepath[MAXFILEPATHSIZE];
		fileStatus fStatus;

		if (errno != 0){
			ERR("readdir");
		}
		if (dirEntry->d_name[0] == '.'){ //Ignore hidden files.
			continue;
		}
		sprintf(filepath, "%s/%s", lArg.dirpath, dirEntry->d_name);
		if (!isRegularFile(filepath)){ //Ignore non-files.
			continue;
		}
		getFileStatus(dirEntry->d_name, &fStatus);

		//Reallocate more memory.
		if (i == n){
			n *= 2;
			if ((fileList = (char*)realloc(fileList, n * sizeof(fileStatus))) == NULL){
				ERR("realloc");
			}
		}
		memcpy(fileList + i * sizeof(fileStatus), &fStatus, sizeof(fileStatus));
		i++;
	}

	//Send metadata.
	metadataMsg.msgType = MSGTYPE_METADATA;
	metadataMsg.totalSize = i * sizeof(fileStatus);
	sendDatagram(sock, lArg.addr, &metadataMsg);

	//Receive metadata response.
	receiveDatagram(sock, NULL, &buffer);
	responseMsg = (responseMessage*)buffer;
	if (responseMsg->msgType != MSGTYPE_RESPONSE){
		ERR("responseMsg->msgType");
	}
	if (responseMsg->error != ERROR_NOERROR){
		fprintf(stderr, "(handleListRequest) Error %d received.\n", responseMsg->error);
		return onExit(sock);
	}
	else{
		//Send list.
		sendData(sock, lArg.addr, fileList, metadataMsg.totalSize);
	}

	free(fileList);
	if (TEMP_FAILURE_RETRY(closedir(dir)) < 0){
		ERR("closedir");
	}
	return onExit(sock);
}

void* handleDownloadRequest(void *arg){
	int sock;
	char filepath[MAXFILEPATHSIZE], buffer[DGRAMSIZE];
	downloadRequestArg dArg;
	fileStatus fStatus;
	responseMessage responseMsg;
	metadataMessage metadataMsg;

	sock = makeUdpSocket(0, TIMEOUT_SHORT);
	memcpy(&dArg, arg, sizeof(downloadRequestArg));
	getFileStatus(dArg.filename, &fStatus);
	sprintf(filepath, "%s/%s", dArg.dirpath, dArg.filename);

	responseMsg.msgType = MSGTYPE_RESPONSE;
	if (fStatus.operation == OPER_UPLOAD){
		responseMsg.error = ERROR_UPLOADING;
		sendDatagram(sock, dArg.addr, &responseMsg);
		return onExit(sock);
	}
	else if (getFileSize(filepath) < 0){
		responseMsg.error = ERROR_NOTFOUND;
		sendDatagram(sock, dArg.addr, &responseMsg);
		return onExit(sock);
	}

	//Send metadata message.
	memcpy(metadataMsg.filename, dArg.filename, MAXFILENAMESIZE);
	metadataMsg.msgType = MSGTYPE_METADATA;
	metadataMsg.totalSize = getFileSize(filepath);
	getMd5Hash(filepath, &(metadataMsg.checksum));
	sendDatagram(sock, dArg.addr, &metadataMsg);

	//Receive metadata response.
	if (receiveDatagram(sock, NULL, buffer) < 0){
		return onExit(sock);
	}
	memcpy(&responseMsg, buffer, sizeof(responseMessage));
	if (responseMsg.msgType != MSGTYPE_RESPONSE){
		ERR("responseMsg.msgType");
	}
	if (responseMsg.error != ERROR_NOERROR){
		fprintf(stderr, "(handleDownloadRequest) Error %d received.\n", responseMsg.error);
		return onExit(sock);
	}

	//Update file metadata (1).
	fStatus.operation = OPER_DOWNLOAD;
	fStatus.totalSize = getFileSize(filepath);
	fStatus.bytesLeft = fStatus.totalSize - dArg.byteOffset;
	fStatus.counter++;
	setFileStatus(&fStatus);

	//Send file data.
	sendFile(sock, dArg.addr, filepath, dArg.byteOffset);

	//Update file metadata (2).
	getFileStatus(dArg.filename, &fStatus);
	if (--fStatus.counter == 0){
		fStatus.operation = OPER_NONE;
	}
	setFileStatus(&fStatus);

	return onExit(sock);
}

void* handleUploadRequest(void *arg){
	int sock;
	char filepath[MAXFILEPATHSIZE], buffer[DGRAMSIZE];
	uploadRequestArg uArg;
	fileStatus fStatus;
	responseMessage responseMsg;
	metadataMessage *metadataMsg;

	sock = makeUdpSocket(0, TIMEOUT_SHORT);
	memcpy(&uArg, arg, sizeof(uploadRequestArg));
	free(arg);
	getFileStatus(uArg.filename, &fStatus);
	sprintf(filepath, "%s/%s", uArg.dirpath, uArg.filename);

	//Check for errors and send appropriate response message.
	responseMsg.msgType = MSGTYPE_RESPONSE;
	if (fStatus.operation == OPER_UPLOAD && fStatus.counter > 0){
		responseMsg.error = ERROR_UPLOADING;
		sendDatagram(sock, uArg.addr, &responseMsg);
		return onExit(sock);
	}
	else if (fStatus.operation == OPER_DOWNLOAD){
		responseMsg.error = ERROR_DOWNLOADING;
		sendDatagram(sock, uArg.addr, &responseMsg);
		return onExit(sock);
	}
	else{
		responseMsg.error = ERROR_NOERROR;
		sendDatagram(sock, uArg.addr, &responseMsg);
	}

	//Receive metadata.
	if (receiveDatagram(sock, NULL, buffer) < 0){
		return onExit(sock);
	}
	metadataMsg = (metadataMessage*)buffer;
	if (metadataMsg->msgType != MSGTYPE_METADATA){
		ERR("metadataMsg->msgType");
	}

	//Update file metadata.
	fStatus.operation = OPER_UPLOAD;
	fStatus.counter = 1;
	fStatus.totalSize = metadataMsg->totalSize;
	fStatus.bytesLeft = fStatus.totalSize - uArg.byteOffset;
	setFileStatus(&fStatus);

	//Truncate file in case the client restarts upload from a different file position.
	truncateFile(filepath, uArg.byteOffset);

	//Send metadata response.
	sendDatagram(sock, uArg.addr, &responseMsg);

	if (receiveFile(sock, filepath) == 0){ //All data has been sent.
		md5hash sum;

		fStatus.operation = OPER_NONE;
		setFileStatus(&fStatus);

		getMd5Hash(filepath, &sum);
		if (!md5cmp(&(metadataMsg->checksum), &sum)){
			fprintf(stdout, "Upload success (%.*s).\n", MAXFILENAMESIZE, uArg.filename);
		}
		else{
			fprintf(stdout, "Upload failure: invalid sum (%.*s).\n", MAXFILENAMESIZE, uArg.filename);
			removeFile(filepath);
		}
	}
	else{ //Upload has been interrupted before all data was sent.
		getFileStatus(uArg.filename, &fStatus);
		fStatus.counter = 0;
		setFileStatus(&fStatus);
	}

	return onExit(sock);
}

void* handleRemoveRequest(void *arg){
	int sock;
	char filepath[MAXFILEPATHSIZE];
	removeRequestArg rArg;
	responseMessage response;
	fileStatus fStatus;

	sock = makeUdpSocket(0, TIMEOUT_SHORT);
	memcpy(&rArg, arg, sizeof(removeRequestArg));
	free(arg);
	getFileStatus(rArg.filename, &fStatus);
	sprintf(filepath, "%s/%s", rArg.dirpath, rArg.filename);

	//Send remove request response.
	response.msgType = MSGTYPE_RESPONSE;
	switch(fStatus.operation){
		case OPER_UPLOAD:
			response.error = ERROR_UPLOADING;
			break;
		case OPER_DOWNLOAD:
			response.error = ERROR_DOWNLOADING;
			break;
		case OPER_NONE:
			if (getFileSize(filepath) < 0){
				response.error = ERROR_NOTFOUND;
			}
			else{
				removeFile(filepath);
				response.error = ERROR_NOERROR;
			}
			break;
	}
	sendDatagram(sock, rArg.addr, &response);

	return onExit(sock);
}

//Common cleanup function.
void* onExit(int sock){
	closeSocket(sock);
	return NULL;
}
