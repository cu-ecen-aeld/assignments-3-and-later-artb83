#include <errno.h>
#include <stdio.h>
#include <syslog.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include "aesdsocket.h"

static bool caught_sigint=false;
static bool caught_sigterm=false;
static char* data=NULL;

void closeAll(int sfd, int cfd, int fd) {
	closelog();
	shutdown(sfd, SHUT_RDWR);
	close(sfd);
	close(cfd);
	close(fd);
	if(data!=NULL)free(data);
}

static void signalHandler(int numOfSignal){
	if( numOfSignal == SIGINT ) caught_sigint = true;
	if( numOfSignal == SIGTERM ) caught_sigterm = true;
}
void writeMsgToSyslog(int log_facility, int log_priority, const char* msgToLog) {
	openlog(NULL, 0, log_facility);
	syslog(log_priority, "%s", msgToLog);
	closelog();
}

int main(int argc, char** argv){
	printf("HELLO  Argc=%d Argv=%s!\n", argc, argv[1]);
	struct sigaction new_action;
	bool bDaemon=(argc>1 ? (strcmp(argv[1], "-d")==0 || strcmp(argv[1], "d")==0) : false);
	bool bRun=true;

	struct addrinfo hints;
	struct addrinfo* servinfo=NULL;
	int srvfd = 0; //server
	int cfd = 0;   //client
	int fd=0;      //file for incomming stream message
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;
	if(0!=getaddrinfo(NULL, "9000", &hints, &servinfo)) {
		printf("Error %d (%s) when getting addrinfo\n", errno, strerror(errno));
		exit(1);
	}

	srvfd=socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);
	if(srvfd<0) {
		printf("Error %d (%s) when creating a socket\n", errno, strerror(errno));
		exit(1);
	}

	int yes=1;
	setsockopt(srvfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));
	if(bind(srvfd, servinfo->ai_addr, servinfo->ai_addrlen)<0) {
		printf("Error %d (%s) when binding a socket\n", errno, strerror(errno));
		exit(1);
	}
	if(servinfo!=NULL)freeaddrinfo(servinfo);

	// allocate data buffer
	if( NULL==( data=(char*)malloc(10*1024*1024*sizeof(char)) ) ) bRun=false;

	if(!bRun) { //if any errors, close all and exit
		closeAll(srvfd, cfd, fd);
		exit(1);
	} else {    //else subscribe to signals, listen for incoming connections and signals, continue running.
		memset(&new_action, 0, sizeof(struct sigaction));
		new_action.sa_handler = signalHandler;
		if( sigaction(SIGINT, &new_action, NULL) !=0 ) {
			printf("Error %d (%s) registering for SIGINT", errno, strerror(errno));
			bRun=false;
			exit(1);
		}
		if( sigaction(SIGTERM, &new_action, NULL) !=0 ) {
			printf("Error %d (%s) registering for SIGTERM", errno, strerror(errno));
			bRun=false;
			exit(1);
		}
	}
	//listen, accept, connect and respond
	socklen_t cAddrLen=0;
	struct sockaddr cInfo;
	//listen
	listen(srvfd, LISTEN_BACKLOG);
	while(bRun) {
		printf("\nWaiting for signal\n");
		if(caught_sigint || caught_sigterm) {
			writeMsgToSyslog(LOG_USER, LOG_INFO, "Caught signal, exiting");
			closeAll(srvfd, cfd, fd);
			bRun=false;
			printf("\nCaught signal, exiting\n");
			exit(0);
		}
		cAddrLen=sizeof(cInfo);
		cfd = accept(srvfd, &cInfo, &cAddrLen);
			openlog(NULL, 0, LOG_USER);
			syslog(LOG_INFO, "Accepted connection from %s", cInfo.sa_data);
			closelog();
		if(cfd>0){
			setsockopt(cfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));
			// if(-1 == connect(cfd, &cInfo, cAddrLen)) {
			// 	printf("Error %d (%s) when connecting a client socket\n", errno, strerror(errno));
			// }
			ssize_t recieved = recv(cfd, data, 1024*1024*sizeof(char), MSG_WAITALL);//MSG_WAITALL
			printf("\nServer got %ld bytes of data: (%s)\n", recieved, data);

			ssize_t sent = send(cfd, data, recieved, 0);//MSG_FASTOPEN
			if(sent==-1) printf("Error %d (%s) when sending data to a client\n", errno, strerror(errno));
			printf("Server sent %ld bytes of data: (%s)\n", sent, data);
			if(bDaemon)printf("is daemon\n");
			shutdown(cfd, SHUT_RDWR);
			close(cfd);
			cfd=-1;
			openlog(NULL, 0, LOG_USER);
			syslog(LOG_INFO, "Closed connection from %s", cInfo.sa_data);
			closelog();
		}
	}
	closeAll(srvfd, cfd, fd);
	return 0;
}