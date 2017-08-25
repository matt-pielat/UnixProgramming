#include "communication.h"
#include "files.h"
#include "libraries.h"
#include "metadata.h"
#include "requests.h"
#include "utilities.h"

int main(int argc, char** argv){
	char *dirpath;
	int mainPort, mainSocket;

	if (argc < 3){
		fprintf(stdout, "USAGE:\n");
		fprintf(stdout, "./server port dir\n");
		fprintf(stdout, "port - listener port\n");
		fprintf(stdout, "dir - path of the directory to share\n");
		return EXIT_FAILURE;
	}

	//Parse the arguments.
	mainPort = atoi(argv[1]);
	dirpath = argv[2];
	if (strlen(dirpath) > MAXDIRPATHSIZE){
		fprintf(stdout, "Directory path is too long.\n");
		fprintf(stdout, "Maximum length is %d characters.\n", MAXDIRPATHSIZE);
		fprintf(stdout, "Terminating...\n");
		return EXIT_FAILURE;
	}

	//Make metafile.
	metaFileInit();

	//Make main socket with no timeout.
	mainSocket = makeUdpSocket(mainPort, -1);

	fprintf(stdout, "Server running...\n");

	for(;;){
		char buffer[DGRAMSIZE];
		requestMessage *requestMsg;
		struct sockaddr_in addr;

		receiveDatagram(mainSocket, &addr, buffer);
		requestMsg = (requestMessage*)buffer;
		if (requestMsg->msgType != MSGTYPE_REQUEST){
			continue;
		}

		fprintf(stderr, "Received [%c] request.\n", requestMsg->request);

		switch (requestMsg->request){
			case REQUEST_HANDSHAKE:{
				handleHandshakeRequest(mainSocket, addr);
				break;
			}
			case REQUEST_LIST:{
				listRequestArg *arg;
				arg = (listRequestArg*)xAlloc(sizeof(listRequestArg));

				arg->addr = addr;
				memcpy(arg->dirpath, dirpath, MAXDIRPATHSIZE);
				runDetachedThread(handleListRequest, arg);
				break;
			}
			case REQUEST_DOWNLOAD:{
				downloadRequestArg *arg;
				arg = (downloadRequestArg*)xAlloc(sizeof(downloadRequestArg));

				arg->addr = addr;
				arg->byteOffset = requestMsg->byteOffset;
				memcpy(arg->filename, requestMsg->filename, MAXFILENAMESIZE);
				memcpy(arg->dirpath, dirpath, MAXDIRPATHSIZE);
				runDetachedThread(handleDownloadRequest, arg);
				break;
			}
			case REQUEST_UPLOAD:{
				uploadRequestArg *arg;
				arg = (uploadRequestArg*)xAlloc(sizeof(uploadRequestArg));

				arg->addr = addr;
				arg->byteOffset = requestMsg->byteOffset;
				memcpy(arg->filename, requestMsg->filename, MAXFILENAMESIZE);
				memcpy(arg->dirpath, dirpath, MAXDIRPATHSIZE);
				runDetachedThread(handleUploadRequest, arg);
				break;
			}
			case REQUEST_DELETE:{
				removeRequestArg *arg;
				arg = (removeRequestArg*)xAlloc(sizeof(removeRequestArg));

				arg->addr = addr;
				memcpy(arg->filename, requestMsg->filename, MAXFILENAMESIZE);
				memcpy(arg->dirpath, dirpath, MAXDIRPATHSIZE);
				runDetachedThread(handleRemoveRequest, arg);
				break;
			}
		}
	}

	return EXIT_SUCCESS;
}


