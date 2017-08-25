/*
 * metadata.h
 *
 *  Created on: Jun 14, 2015
 *      Author: matthew
 */

#ifndef METADATA_H_
#define METADATA_H_

#include "files.h"

#define OPER_NONE		0x0 //Neither being downloaded nor uploaded. To overwrite.
#define OPER_DOWNLOAD	0x1
#define OPER_UPLOAD		0x2
#define OPER_SENTINEL	0x3 //Indicates the end of meta file.

typedef struct{
	char filename[MAXFILENAMESIZE];
	size_t totalSize;
	size_t bytesLeft; //Applicable if operation is set to either OPER_DOWNLOAD or OPER_UPLOAD
	int	counter; //Number of clients currently uploading/downloading the file (max value of 1 on client's side).
	int operation; //Current operation (one of the OPER_X values).
} fileStatus;

void metaFileInit();
void resetCounters();
void setFileStatus(fileStatus *fs);
void getFileStatus(char *filename, fileStatus *fs);
int getOngoingOperation(int offset, fileStatus *fs);

#endif /* METADATA_H_ */
