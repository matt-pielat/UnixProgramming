/*
 * files.c
 *
 *  Created on: Jun 6, 2015
 *      Author: matthew
 */

#include "files.h"
#include "utilities.h"

//Returns -1 only in case of EEXIST (O_CREAT & O_EXCL set and file already exists).
int openFile(char* filepath, int oflag){
	int fd;
	if (oflag && O_CREAT){
		fd = TEMP_FAILURE_RETRY(open(filepath, oflag, 0777));
	}
	else{
		fd = TEMP_FAILURE_RETRY(open(filepath, oflag));
	}
	if (fd < 0 && errno != EEXIST){
		ERR("open");
	}
	return fd;
}

void closeFile(int fd){
	if (TEMP_FAILURE_RETRY(close(fd)) < 0){
		ERR("close");
	}
}

int isRegularFile(char *filepath){
	struct stat s;
	if (lstat(filepath, &s) < 0){
		ERR("lstat");
	}
	return S_ISREG(s.st_mode);
}

int removeFile(char *filepath){
	if (unlink(filepath) < 0){
		if (errno == ENOENT){
			return -1;
		}
		ERR("unlink");
	}
	return 0;
}

void truncateFile(char *filepath, off_t len){
	int fd = openFile(filepath, O_WRONLY|O_CREAT);
	closeFile(fd);
	if (TEMP_FAILURE_RETRY(truncate(filepath, len)) < 0){
		ERR("truncate");
	}
}

//Returns -1 if file doesn't exist.
off_t getFileSize(char *filepath){
	struct stat st;
	int status;
	if ((status = lstat(filepath, &st)) < 0){
		if (errno == ENOENT){
			return -1;
		}
		ERR("lstat");
	}
	return st.st_size;
}

void getMd5Hash(char *filepath, md5hash *output){
	int fd;
	MD5_CTX ctx;
	char buf[1024];
	ssize_t bytesRead;

	fd = openFile(filepath, O_RDONLY);

	if (!MD5_Init(&ctx)){
		ERR("MD5Init");
	}
	do{
		if((bytesRead = bulkRead(fd, buf, 1024)) < 0){
			ERR("bulkRead");
		}
		if (!MD5_Update(&ctx, buf, bytesRead)){
			ERR("MD5Update");
		}
	}while(bytesRead == 1024);
	if (!MD5_Final(output->hash, &ctx)){
		ERR("MD5Final");
	}

	closeFile(fd);
}

int md5cmp(md5hash *a, md5hash *b){
	int i;
	for (i = 0; i < MD5_DIGEST_LENGTH; i++){
		if (a->hash[i] != b->hash[i]){
			return -1;
		}
	}
	return 0;
}
