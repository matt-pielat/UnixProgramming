/*
 * utilities.h
 *
 *  Created on: Jun 6, 2015
 *      Author: matthew
 */

#ifndef UTILITIES_H_
#define UTILITIES_H_

ssize_t bulkRead(int fd, void *buf, size_t count);
ssize_t bulkWrite(int fd, void *buf, size_t count);
ssize_t bulkPread(int fd, void *buf, size_t count, off_t offset);
ssize_t bulkPwrite(int fd, void *buf, size_t count, off_t offset);
void runDetachedThread(void *(*routine)(void*), void *arg);
char getByteInput();
void getInput(char* input);
void setSighandler(void(*f)(int), int signum);
void xSleep(unsigned int seconds);
void* xAlloc(size_t size);

#endif /* UTILITIES_H_ */
