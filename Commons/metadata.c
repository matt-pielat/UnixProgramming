/*
 * metadata.c
 *
 *  Created on: Jun 14, 2015
 *      Author: matthew
 */

#include "metadata.h"
#include "utilities.h"

#define METAFILEPATH	"./metadata"

static pthread_mutex_t metaFileLock;

//Must be synchronously called once on client/server start.
void metaFileInit(){
	int fd;
	pthread_mutexattr_t attr;
	if (pthread_mutexattr_init(&attr)){
		ERR("pthread_mutexattr_init");
	}
	if (pthread_mutex_init(&metaFileLock, &attr)){
		ERR("pthread_mutex_init");
	}
	fd = openFile(METAFILEPATH, O_RDWR|O_CREAT);
	if (getFileSize(METAFILEPATH) == 0){
		fileStatus sentinel;
		sentinel.operation = OPER_SENTINEL;
		if (bulkWrite(fd, &sentinel, sizeof(fileStatus)) < 0){
			ERR("bulkWrite");
		}
	}
	closeFile(fd);
	resetCounters();
}

void resetCounters(){
	int fd;
	if (pthread_mutex_lock(&metaFileLock)){
		ERR("pthread_mutex_lock");
	}
	fd = openFile(METAFILEPATH, O_RDWR);
	while (1){
		fileStatus buf;
		off_t prevPos;
		if ((prevPos = lseek(fd, 0, SEEK_CUR)) < 0){
			ERR("lseek");
		}
		if (bulkRead(fd, &buf, sizeof(fileStatus)) < 0){
			ERR("bulkRead");
		}
			if (buf.operation == OPER_SENTINEL){
			break;
		}
		buf.counter = 0;
		bulkPwrite(fd, &buf, sizeof(fileStatus), prevPos);
	}
	if (pthread_mutex_unlock(&metaFileLock)){
		ERR("pthread_mutex_unlock");
	}
}

void setFileStatus(fileStatus *fs){
	int fd;
	off_t availableEntryPos = -1, prevPos;
	fileStatus buf;
	if (pthread_mutex_lock(&metaFileLock)){
		ERR("pthread_mutex_lock");
	}
	fd = openFile(METAFILEPATH, O_RDWR);
	while (1){
		if ((prevPos = lseek(fd, 0, SEEK_CUR)) < 0){
			ERR("lseek");
		}
		if (bulkRead(fd, &buf, sizeof(fileStatus)) < 0){
			ERR("bulkRead");
		}
		if (buf.operation == OPER_SENTINEL){
			break;
		}
		if (!strcmp(fs->filename, buf.filename)){
			if (bulkPwrite(fd, fs, sizeof(fileStatus), prevPos) < 0){
				ERR("bulkPWrite");
			}
			break;
		}
		if (availableEntryPos == -1 && buf.operation == OPER_NONE){
			availableEntryPos = prevPos;
		}
	}
	if (buf.operation == OPER_SENTINEL){
		if (availableEntryPos == -1){
			if (bulkWrite(fd, &buf, sizeof(fileStatus)) < 0){ //New sentinel.
				ERR("bulkWrite");
			}
			if (bulkPwrite(fd, fs, sizeof(fileStatus), prevPos) < 0){ //Overwrite old sentinel.
				ERR("bulkWrite");
			}
		}
		else{
			if (bulkPwrite(fd, fs, sizeof(fileStatus), availableEntryPos) < 0){
				ERR("bulkPWrite");
			}
		}
	}
	closeFile(fd);
	if (pthread_mutex_unlock(&metaFileLock)){
		ERR("pthread_mutex_unlock");
	}
}

//If returned fileStatus has OPER_NONE operation set, fileStatus.totalSize is not to be relied upon.
void getFileStatus(char *filename, fileStatus *fs){
	int fd;
	fileStatus buf;
	if (pthread_mutex_lock(&metaFileLock)){
		ERR("pthread_mutex_lock");
	}
	fd = openFile(METAFILEPATH, O_RDONLY);
	while (1){
		if (bulkRead(fd, &buf, sizeof(fileStatus)) < 0){
			ERR("bulkRead");
		}
		if (buf.operation == OPER_SENTINEL){
			break;
		}
		if (!strcmp(filename, buf.filename)){
			memcpy(fs, &buf, sizeof(fileStatus));
			break;
		}
	}
	if (buf.operation == OPER_SENTINEL){
		memcpy(fs->filename, filename, MAXFILENAMESIZE);
		fs->operation = OPER_NONE;
	}
	closeFile(fd);
	if (pthread_mutex_unlock(&metaFileLock)){
		ERR("pthread_mutex_unlock");
	}
}

int getOngoingOperation(int offset, fileStatus *fs){
	int fd, i = 0;
	fileStatus buf;
	if (pthread_mutex_lock(&metaFileLock)){
		ERR("pthread_mutex_lock");
	}
	fd = openFile(METAFILEPATH, O_RDONLY);
	while (1){
		if (bulkRead(fd, &buf, sizeof(fileStatus)) < 0){
			ERR("bulkRead");
		}
		if (buf.operation == OPER_SENTINEL){
			i = -1;
			break;
		}
		if (i >= offset){
			if (buf.operation == OPER_DOWNLOAD ||
				buf.operation == OPER_UPLOAD){
				memcpy(fs, &buf, sizeof(fileStatus));
				break;
			}
		}
		i++;
	}
	closeFile(fd);
	if (pthread_mutex_unlock(&metaFileLock)){
		ERR("pthread_mutex_unlock");
	}
	return i;
}
