/*
 * utilities.c
 *
 *  Created on: Jun 6, 2015
 *      Author: matthew
 */

#include "libraries.h"
#include "utilities.h"

ssize_t bulkRead(int fd, void *buf, size_t count){
	int c;
	size_t len = 0;
	do{
		c = TEMP_FAILURE_RETRY(read(fd, buf, count));
		if (c < 0) {
			return c;
		}
		if (0 == c) {
			return len;
		}
		buf += c;
		len += c;
		count -= c;
	}while(count > 0);
	return len ;
}

ssize_t bulkWrite(int fd, void *buf, size_t count){
	int c;
	size_t len = 0;
	do{
		c = TEMP_FAILURE_RETRY(write(fd, buf, count));
		if (c < 0){
			return c;
		}
		buf += c;
		len += c;
		count -= c;
	}while(count > 0);
	return len ;
}

ssize_t bulkPread(int fd, void *buf, size_t count, off_t offset){
	int c;
	off_t o = offset;
	size_t len = 0;
	do{
		c = TEMP_FAILURE_RETRY(pread(fd, buf, count, o));
		if (c < 0) {
			return c;
		}
		if (0 == c) {
			return len;
		}
		o += c;
		buf += c;
		len += c;
		count -= c;
	}while(count > 0);
	return len ;
}

ssize_t bulkPwrite(int fd, void *buf, size_t count, off_t offset){
	int c;
	off_t o = offset;
	size_t len = 0;
	do{
		c = TEMP_FAILURE_RETRY(pwrite(fd, buf, count, o));
		if (c < 0){
			return c;
		}
		o += c;
		buf += c;
		len += c;
		count -= c;
	}while(count > 0);
	return len ;
}

void runDetachedThread(void *(*routine)(void*), void *arg){
	pthread_attr_t attr;
	pthread_t th;
	if (pthread_attr_init(&attr)){
		ERR("pthread_attr_init");
	}
	if (pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED)){
		ERR("pthread_attr_setdetachstate");
	}
	if (pthread_create(&th, &attr, routine, arg)){
		ERR("pthread_create");
	}
}

char getByteInput(){
	char input, c;
	if ((input = TEMP_FAILURE_RETRY(getchar())) == EOF){
		ERR("getchar");
	}
	while((c = getchar()) != '\n' && c != EOF); //Clean stdin.
	return input;
}

void getInput(char* input){
	char c;
	if (TEMP_FAILURE_RETRY(scanf("%s", input)) < 1){
		ERR("scanf");
	}
	while((c = getchar()) != '\n' && c != EOF); //Clean stdin.
}

void setSighandler(void(*f)(int), int signum)
{
	struct sigaction act;
	memset(&act, 0, sizeof(struct sigaction));
	act.sa_handler = f;
	if (sigaction(signum, &act, NULL) < 0){
		ERR("sigaction");
	}
}

void xSleep(unsigned int seconds){
	int t;
	for (t = seconds; t > 0; t = sleep(t));
}

void* xAlloc(size_t size){
	void *ptr;
	if ((ptr = malloc(size)) < 0){
		ERR("malloc");
	}
	return ptr;
}
