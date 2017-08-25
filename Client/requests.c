/*
 * commands.c
 *
 *  Created on: Jun 13, 2015
 *      Author: matthew
 */

#include "communication.h"
#include "requests.h"
#include "metadata.h"

void* listRequest(void *arg){
	int sock, i, numOfFiles;
	char *fileList, buffer[DGRAMSIZE];
	struct sockaddr_in addr;
	listRequestArg lArg;
	requestMessage requestMsg;
	responseMessage responseMsg;
	metadataMessage *metadataMsg;

	sock = makeUdpSocket(0, TIMEOUT_SHORT);
	memcpy(&lArg, arg, sizeof(listRequestArg));
	free(arg);

	//Send list request message.
	requestMsg.msgType = MSGTYPE_REQUEST;
	requestMsg.request = REQUEST_LIST;
	sendDatagram(sock, lArg.serverAddr, &requestMsg);

	//Receive metadata message.
	if (receiveDatagram(sock, &addr, buffer) < 0){
		fprintf(stderr, "(listRequest) Timeout.\n");
		return onExit(sock, 1);
	}
	metadataMsg = (metadataMessage*)buffer;
	if (metadataMsg->msgType != MSGTYPE_METADATA){
		ERR("metadataMsg->msgType");
	}

	//Allocate memory for the list.
	numOfFiles = metadataMsg->totalSize / sizeof(fileStatus);
	fileList = xAlloc(metadataMsg->totalSize);

	//Send metadata response message.
	responseMsg.msgType = MSGTYPE_RESPONSE;
	responseMsg.error = ERROR_NOERROR;
	sendDatagram(sock, addr, &responseMsg);

	//Receive list of files on the server.
	if (receiveData(sock, fileList, metadataMsg->totalSize) < metadataMsg->totalSize){
		fprintf(stderr, "(listRequest) Received less data than expected.\n");
		free(fileList);
		return onExit(sock, 1);
	}

	for (i = 0; i < numOfFiles; i++){
		fileStatus *serverStatus;
		fileStatus localStatus;

		serverStatus = (fileStatus*)(fileList + i * sizeof(fileStatus));
		getFileStatus(serverStatus->filename, &localStatus);

		//Don't print other clients' ongoing uploads.
		if (localStatus.operation != OPER_UPLOAD &&
			serverStatus->operation == OPER_UPLOAD)
			continue;

		fprintf(stdout, "\n%d.\t%.*s", i + 1, MAXFILENAMESIZE, serverStatus->filename);

		if (localStatus.operation == OPER_DOWNLOAD){
			int progress = (localStatus.totalSize - localStatus.bytesLeft) * 100 / localStatus.totalSize;
			fprintf(stdout, " [downloading %d%%]", progress);
		}
		else if (localStatus.operation == OPER_UPLOAD){
			int progress = (localStatus.totalSize - localStatus.bytesLeft) * 100 / localStatus.totalSize;
			fprintf(stdout, " [uploading %d%%]", progress);
		}
	}
	fprintf(stdout, "\n");

	free(fileList);
	return onExit(sock, 0);
}

void* downloadRequest(void *arg){
	int sock;
	char buffer[DGRAMSIZE], filepath[MAXFILEPATHSIZE];
	struct sockaddr_in addr;
	off_t offset;
	downloadRequestArg dArg;
	fileStatus fStatus;
	requestMessage requestMsg;
	metadataMessage *metadataMsg;
	responseMessage responseMsg;

	sock = makeUdpSocket(0, TIMEOUT_SHORT);
	memcpy(&dArg, arg, sizeof(downloadRequestArg));
	free(arg);
	getFileStatus(dArg.filename, &fStatus);
	sprintf(filepath, "%s/%s", dArg.dirpath, dArg.filename);

	if (fStatus.operation == OPER_DOWNLOAD){
		if (fStatus.counter){ //Client has already resumed this file download.
			fprintf(stdout, "Already downloading this file.\n");
			return onExit(sock, 0);
		}
		if ((offset = getFileSize(filepath)) < 0){
			offset = 0;
		}
		fStatus.counter = 1;
	}
	else if (fStatus.operation == OPER_NONE){
		removeFile(filepath); //Overwrite existing file.
		fStatus.operation = OPER_DOWNLOAD;
		offset = 0;
		fStatus.counter = 1;
	}
	else{
		fprintf(stdout, "This file is being uploaded.\n");
		return onExit(sock, 0);
	}

	//Send request message.
	requestMsg.msgType = MSGTYPE_REQUEST;
	requestMsg.request = REQUEST_DOWNLOAD;
	requestMsg.byteOffset = offset;
	memcpy(requestMsg.filename, dArg.filename, MAXFILENAMESIZE);
	sendDatagram(sock, dArg.serverAddr, &requestMsg);

	//Receive metadata (or error) message.
	if (receiveDatagram(sock, &addr, buffer) < 0){
		fprintf(stderr, "(downloadRequest) Timeout.\n");
		return onExit(sock, 1);
	}
	metadataMsg = (metadataMessage*)buffer;
	if (metadataMsg->msgType == MSGTYPE_RESPONSE){ //Received response message (error) instead of metadata.
		memcpy(&responseMsg, metadataMsg, sizeof(responseMessage));
		if (responseMsg.error == ERROR_NOTFOUND ||
			responseMsg.error == ERROR_UPLOADING){
			fprintf(stdout, "File %.*s not found on the server.\n", MAXFILENAMESIZE, dArg.filename);
		}
		else{
			fprintf(stderr, "(downloadRequest) Error %d received.\n", responseMsg.error);
		}
		return onExit(sock, 0);
	}
	else if (metadataMsg->msgType != MSGTYPE_METADATA){
		ERR("metadataMsg->msgType");
	}

	//Update local metadata info.
	fStatus.totalSize = metadataMsg->totalSize;
	fStatus.bytesLeft = fStatus.totalSize - offset;
	setFileStatus(&fStatus);

	//Send metadata response message.
	responseMsg.msgType = MSGTYPE_RESPONSE;
	responseMsg.error = ERROR_NOERROR;
	sendDatagram(sock, addr, &responseMsg);

	//Receive file.
	if (!receiveFile(sock, filepath)){
		md5hash sum;

		fStatus.operation = OPER_NONE;
		setFileStatus(&fStatus);

		getMd5Hash(filepath, &sum);
		if (!md5cmp(&(metadataMsg->checksum), &sum)){
			fprintf(stdout, "Download success (%.*s).\n", MAXFILENAMESIZE, dArg.filename);
		}
		else{
			fprintf(stdout, "Download failure - invalid md5 sum (%.*s).\n", MAXFILENAMESIZE, dArg.filename);
			removeFile(filepath);
		}
	}
	else{
		return onExit(sock, 1);
	}

	return onExit(sock, 0);
}

void* uploadRequest(void *arg){
	int sock;
	char buffer[DGRAMSIZE], filepath[MAXFILEPATHSIZE];
	struct sockaddr_in addr;
	off_t offset;
	uploadRequestArg uArg;
	fileStatus fStatus;
	requestMessage requestMsg;
	metadataMessage metadataMsg;
	responseMessage *responseMsg;

	sock = makeUdpSocket(0, TIMEOUT_SHORT);
	memcpy(&uArg, arg, sizeof(uploadRequestArg));
	free(arg);
	getFileStatus(uArg.filename, &fStatus);
	sprintf(filepath, "%s/%s", uArg.dirpath, uArg.filename);

	//Check if file exists.
	if (getFileSize(filepath) < 0){
		fprintf(stdout, "File not found.\n");
		return onExit(sock, 0);
	}

	if (fStatus.operation == OPER_UPLOAD){
		if (fStatus.counter){
			//Actually, the control will never end up here (this case is handled later in the code).
			fprintf(stdout, "Already uploading this file.\n");
			return onExit(sock, 0);
		}
		offset = fStatus.totalSize - fStatus.bytesLeft;
	}
	else if (fStatus.operation == OPER_NONE){
		fStatus.operation = OPER_UPLOAD;
		fStatus.totalSize = fStatus.bytesLeft = getFileSize(filepath);
		offset = 0;
	}
	else{
		fprintf(stdout, "This file is being downloaded.\n");
		return onExit(sock, 0);
	}

	//Send request message.
	requestMsg.msgType = MSGTYPE_REQUEST;
	requestMsg.request = REQUEST_UPLOAD;
	requestMsg.byteOffset = offset;
	memcpy(requestMsg.filename, uArg.filename, MAXFILENAMESIZE);
	sendDatagram(sock, uArg.serverAddr, &requestMsg);

	//Receive response (check for errors).
	if (receiveDatagram(sock, &addr, buffer) < 0){
		return onExit(sock, 1);
	}
	responseMsg = (responseMessage*)buffer;
	if (responseMsg->msgType != MSGTYPE_RESPONSE){
		ERR("responseMsg->msgType");
	}
	switch (responseMsg->error){
		case ERROR_DOWNLOADING:
			fprintf(stdout, "Cannot upload %.*s - file is being downloaded.\n",
					MAXFILENAMESIZE, uArg.filename);
			return onExit(sock, 0);
		case ERROR_UPLOADING:
			fprintf(stdout, "Cannot upload %.*s - file with such name is already being uploaded.\n",
					MAXFILENAMESIZE, uArg.filename);
			return onExit(sock, 0);
	}

	//Update local metadata info.
	setFileStatus(&fStatus);

	//Send metadata.
	metadataMsg.msgType = MSGTYPE_METADATA;
	metadataMsg.totalSize = fStatus.totalSize;
	getMd5Hash(filepath, &(metadataMsg.checksum));
	memcpy(metadataMsg.filename, uArg.filename, MAXFILENAMESIZE);
	sendDatagram(sock, addr, &metadataMsg);

	//Receive metadata response.
	if (receiveDatagram(sock, NULL, buffer) < 0){
		return onExit(sock, 1);
	}
	responseMsg = (responseMessage*)buffer;
	if (responseMsg->msgType != MSGTYPE_RESPONSE){
		ERR("responseMsg->msgType");
	}
	if (responseMsg->error != ERROR_NOERROR){
		ERR("responseMsg->error");
	}

	//Send file.
	if (!sendFile(sock, addr, filepath, offset)){
		fStatus.operation = OPER_NONE;
		setFileStatus(&fStatus);

		fprintf(stdout, "Upload completed (%.*s).\n", MAXFILENAMESIZE, uArg.filename);
	}
	else{
		return onExit(sock, 1);
	}

	return onExit(sock, 0);
}

void* removeRequest(void *arg){
	int sock;
	char buffer[DGRAMSIZE];
	removeRequestArg rArg;
	requestMessage requestMsg;
	responseMessage *responseMsg;

	sock = makeUdpSocket(0, TIMEOUT_SHORT);
	memcpy(&rArg, arg, sizeof(removeRequestArg));
	free(arg);

	//Send remove request message.
	requestMsg.msgType = MSGTYPE_REQUEST;
	requestMsg.request = REQUEST_DELETE;
	memcpy(requestMsg.filename, rArg.filename, MAXFILENAMESIZE);
	sendDatagram(sock, rArg.serverAddr, &requestMsg);

	//Receive remove request response.
	if (receiveDatagram(sock, NULL, buffer) < 0){
		return onExit(sock, 1);
	}
	responseMsg = (responseMessage*)buffer;
	if (responseMsg->msgType != MSGTYPE_RESPONSE){
		ERR("responseMsg->msgType");
	}

	//Check operation status and print appropriate message.
	switch (responseMsg->error){
		case ERROR_NOERROR:
			fprintf(stdout, "%s successfully deleted from the server.\n", rArg.filename);
			break;
		case ERROR_DOWNLOADING:
			fprintf(stdout, "Cannot delete. File is being downloaded.\n");
			break;
		case ERROR_UPLOADING:
			fprintf(stdout, "Cannot delete. File is being uploaded.\n");
			break;
		case ERROR_NOTFOUND:
			fprintf(stdout, "Cannot delete. Such file doesn't exist on the server.\n");
			break;
	}

	return onExit(sock, 0);
}

//Common cleanup function.
void* onExit(int sock, int timeout){
	closeSocket(sock);
	if (timeout){
		if (kill(getpid(), SIGUSR1) < 0){
			ERR("kill");
		}
	}
	return NULL;
}
