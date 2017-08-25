/*
 * files.h
 *
 *  Created on: Jun 6, 2015
 *      Author: matthew
 */

#ifndef FILES_H_
#define FILES_H_

#include "libraries.h"

#define MAXDIRPATHSIZE	256
#define MAXFILENAMESIZE	64
#define MAXFILEPATHSIZE	(MAXDIRPATHSIZE + MAXFILENAMESIZE + 1)

typedef struct{
	unsigned char hash[MD5_DIGEST_LENGTH];
} md5hash;

int openFile(char* filename, int oflag);
void closeFile(int fd);
int isRegularFile(char *filepath);
int removeFile(char *filepath);
void truncateFile(char *filepath, off_t len);
off_t getFileSize(char *filepath);
void getMd5Hash(char *filepath, md5hash *output);
int md5cmp(md5hash *a, md5hash *b);

#endif /* FILES_H_ */
