/*
 * libraries.h
 *
 *  Created on: Jun 6, 2015
 *      Author: matthew
 */

#ifndef LIBRARIES_H_
#define LIBRARIES_H_

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <netinet/in.h>
#include <openssl/md5.h>
#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>
#include <ctype.h>
#include <unistd.h>

#define ERR(source) (perror(source),\
		fprintf(stderr,"%s:%d\n",__FILE__,__LINE__),\
		exit(EXIT_FAILURE))

#define TEMP_FAILURE_RETRY(expression)\
		(__extension__ ({ long int __result;\
		do __result = (long int) (expression);\
		while (__result == -1L && errno == EINTR);\
		__result; }))


#endif /* LIBRARIES_H_ */
