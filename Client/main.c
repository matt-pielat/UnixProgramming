#include "communication.h"
#include "libraries.h"
#include "metadata.h"
#include "requests.h"
#include "utilities.h"

#define CLIENTDIRPATH "."

void printCommands();
void findOutServerAddress();
void resumeOperations();
void handleTimeout(int sig);

static struct sockaddr_in serverAddr;
static int serverMainPort;
static pthread_mutex_t timeoutHandlingLock;

int main(int argc, char** argv){
	int shouldExit;
	pthread_mutexattr_t attr;

	if (argc < 2){
		fprintf(stdout, "USAGE:\n");
		fprintf(stdout, "./client port\n");
		fprintf(stdout, "port - server listener port\n");
		return EXIT_FAILURE;
	}

	//Parse the arguments.
	serverMainPort = atoi(argv[1]);

	//Set up signal handling.
	setSighandler(handleTimeout, SIGUSR1);
	if (pthread_mutexattr_init(&attr)){
		ERR("pthread_mutexattr_init");
	}
	if (pthread_mutex_init(&timeoutHandlingLock, &attr)){
		ERR("pthread_mutex_init");
	}

	//Make metafile.
	metaFileInit();

	//Find the server.
	findOutServerAddress(serverMainPort, &serverAddr);

	fprintf(stdout, "\n");
	printCommands();

	resumeOperations(serverAddr);

	shouldExit = 0;

	while (!shouldExit){
		char filename[MAXFILENAMESIZE];

		switch (tolower(getByteInput())){
			case CMD_LIST:{
				listRequestArg *arg;
				arg = (listRequestArg*)xAlloc(sizeof(listRequestArg));

				arg->serverAddr = serverAddr;
				runDetachedThread(listRequest, arg);
				break;
			}
			case CMD_DOWNLOAD:{
				downloadRequestArg *arg;
				arg = (downloadRequestArg*)xAlloc(sizeof(downloadRequestArg));

				fprintf(stdout, "Enter filename:\n");
				getInput(filename);
				memcpy(arg->filename, filename, MAXFILENAMESIZE);
				memcpy(arg->dirpath, CLIENTDIRPATH, MAXDIRPATHSIZE);
				arg->serverAddr = serverAddr;
				runDetachedThread(downloadRequest, arg);
				break;
			}
			case CMD_UPLOAD:{
				uploadRequestArg *arg;
				arg = (uploadRequestArg*)xAlloc(sizeof(uploadRequestArg));

				fprintf(stdout, "Enter filename:\n");
				getInput(filename);
				memcpy(arg->filename, filename, MAXFILENAMESIZE);
				memcpy(arg->dirpath, CLIENTDIRPATH, MAXDIRPATHSIZE);
				arg->serverAddr = serverAddr;
				runDetachedThread(uploadRequest, arg);
				break;
			}
			case CMD_DELETE:{
				removeRequestArg *arg;
				arg = (removeRequestArg*)xAlloc(sizeof(removeRequestArg));

				fprintf(stdout, "Enter filename:\n");
				getInput(filename);
				memcpy(arg->filename, filename, MAXFILENAMESIZE);
				arg->serverAddr = serverAddr;
				runDetachedThread(removeRequest, arg);
				break;
			}
			case CMD_QUIT:{
				shouldExit = 1;
				break;
			}
		}
	}

	return EXIT_SUCCESS;
}

void printCommands(){
	fprintf(stdout, "COMMANDS\n");
	fprintf(stdout, "[%c] - list files\n", CMD_LIST);
	fprintf(stdout, "[%c] - download\n", CMD_DOWNLOAD);
	fprintf(stdout, "[%c] - upload\n", CMD_UPLOAD);
	fprintf(stdout, "[%c] - remove\n", CMD_DELETE);
	fprintf(stdout, "[%c] - quit\n", CMD_QUIT);
}

void findOutServerAddress(){
	int sock, yes = 1;
	char buffer[DGRAMSIZE];
	requestMessage handshakeMsg;

	sock = makeUdpSocket(0, TIMEOUT_MEDIUM);
	if (setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &yes, sizeof(int)) < 0){
		ERR("setsockopt");
	}

	handshakeMsg.msgType = MSGTYPE_REQUEST;
	handshakeMsg.request = REQUEST_HANDSHAKE;

	do{
		fprintf(stdout, "Looking for the server...\n");
		sendBroadcastDatagram(sock, serverMainPort, &handshakeMsg);
	} while(receiveDatagram(sock, &serverAddr, buffer) < 0); //While timeout occurs.

	fprintf(stdout, "Server found.\n");
	closeSocket(sock);
}

void resumeOperations(){
	int offset = 0;

	while (1){
		fileStatus buf;

		if ((offset = getOngoingOperation(offset, &buf)) < 0){
			break;
		}

		if (buf.operation == OPER_DOWNLOAD){
			downloadRequestArg *dArg;

			dArg = (downloadRequestArg*)xAlloc(sizeof(downloadRequestArg));
			memcpy(dArg->filename, buf.filename, MAXFILENAMESIZE);
			memcpy(dArg->dirpath, CLIENTDIRPATH, MAXDIRPATHSIZE);
			dArg->serverAddr = serverAddr;

			runDetachedThread(downloadRequest, dArg);
			fprintf(stdout, "Resumed download (%.*s).\n", MAXFILENAMESIZE, buf.filename);
		}
		else if (buf.operation == OPER_UPLOAD){
			uploadRequestArg *uArg;

			uArg = (uploadRequestArg*)xAlloc(sizeof(uploadRequestArg));
			memcpy(uArg->filename, buf.filename, MAXFILENAMESIZE);
			memcpy(uArg->dirpath, CLIENTDIRPATH, MAXDIRPATHSIZE);
			uArg->serverAddr = serverAddr;

			runDetachedThread(uploadRequest, uArg);
			fprintf(stdout, "Resumed upload (%.*s).\n", MAXFILENAMESIZE, buf.filename);
		}

		offset++;
	}

}

void handleTimeout(int sig){
	int error;
	if ((error = pthread_mutex_trylock(&timeoutHandlingLock))){
		if (error == EBUSY){
			return;
		}
		ERR("pthread_mutex_trylock");
	}
	findOutServerAddress();
	resetCounters();
	resumeOperations();
	if (pthread_mutex_unlock(&timeoutHandlingLock)){
		ERR("pthread_mutex_unlock");
	}
}
