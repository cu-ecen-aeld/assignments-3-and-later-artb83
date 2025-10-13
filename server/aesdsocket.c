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
#include <sys/stat.h>
#include <netdb.h>
#include <arpa/inet.h> // for inet_ntop
#include <fcntl.h>
#include <linux/limits.h>
#include "aesdsocket.h"

#include <linux/limits.h>

static bool caught_sigint=false;
static bool caught_sigterm=false;
static char* data=NULL;

void closeAll(int sfd, int cfd, int fd) {
	closelog();
	shutdown(sfd, SHUT_RDWR);
	close(sfd);
	close(cfd);
	close(fd);
	remove("/var/tmp/aesdsocketdata");
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

int appendtofile(int* fd, char* data) {
	char* newline=strstr(data, "\n");
	if( newline!=NULL && 0<(*fd = open("/var/tmp/aesdsocketdata", O_CREAT|O_APPEND|O_WRONLY, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP)) ){
		data[newline-data+1]='\0';
		int res=write(*fd, data, (newline-data+1)*sizeof(char));
		close(*fd);
		return res<0 ? res : 0;
	}
	return -1;
}
ssize_t appendFromFileToBuffAndSend(int cfd, int* fd, char* buff) {
	int nRead = 0;
	ssize_t res=-1;
	if( 0<(*fd = open("/var/tmp/aesdsocketdata", O_RDONLY, S_IRUSR|S_IRGRP)) ){
		res=0;
		while( 0<(nRead = read(*fd, buff, BUFFER_SIZE)) ) {
			if( 0<(res=send(cfd, buff, nRead, 0)) ) break; //MSG_FASTOPEN
		}
	}
	close(*fd);
	return res;
}

int daemonize(void){
	pid_t pid = fork();
	if(pid<0) {
		return -1;
	}else if (pid>0) exit(EXIT_SUCCESS);//exit parent proc

	if (setsid() ==-1) return -1; //create new session and proc group

	sigsubscribe(signalHandler);

	pid = fork();
	if (pid<0) exit(EXIT_FAILURE);
	if (pid>0) exit(EXIT_SUCCESS);
	umask(0);
	chdir("/");
	for (int i=0; i<NR_OPEN; i++) close(i); //closing all file descriptors, including stdin/out/err
	//reopen stdin/out/err and redirect them to /dev/null
	open("/dev/null", O_RDWR);
	dup(0);
	dup(0);
	return 0;
}

int sigsubscribe(void* handler) {
	struct sigaction new_action;

	memset(&new_action, 0, sizeof(struct sigaction));
	new_action.sa_handler = handler;
	if( sigaction(SIGINT, &new_action, NULL) !=0 ) {
		printf("Error %d (%s) registering for SIGINT", errno, strerror(errno));
		return -1;
	}
	if( sigaction(SIGTERM, &new_action, NULL) !=0 ) {
		printf("Error %d (%s) registering for SIGTERM", errno, strerror(errno));
		return  -1;
	}
	return 0;
}

int main(int argc, char** argv){
	printf("HELLO  Argc=%d Argv=%s!\n", argc, argv[1]);
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
	int rv=0;
	setsockopt(srvfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));
	rv=bind(srvfd, servinfo->ai_addr, servinfo->ai_addrlen);
	if(servinfo!=NULL)freeaddrinfo(servinfo);
	if( rv<0 ) {
		printf("Error %d (%s) when binding a socket\n", errno, strerror(errno));
		exit(1);
	}

	// allocate data buffer
	if( NULL==( data=(char*)malloc((1+BUFFER_SIZE)*sizeof(char)) ) ) bRun=false;

	if(!bRun) { //if any errors, close all and exit
		closeAll(srvfd, cfd, fd);
		exit(1);
	} else {    //else subscribe to signals, listen for incoming connections and signals, continue running.
		if(bDaemon) {
			//bRun = (daemonize() == 0 ? true : false);
			daemonize();
		}else {
			bRun = (0==sigsubscribe(signalHandler) ? true : false);
		}
	}
	//listen, accept, connect and respond
	socklen_t cAddrLen=0;
	struct sockaddr_in cInfo;
	char ip4add[INET_ADDRSTRLEN]; //ipv4
	//listen
	if(bRun) listen(srvfd, LISTEN_BACKLOG);
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
		cfd = accept(srvfd, (struct sockaddr*)&cInfo, &cAddrLen);
		if(cfd>0){
			inet_ntop(AF_INET, &cInfo.sin_addr, ip4add, INET_ADDRSTRLEN);
			openlog(NULL, 0, LOG_USER);
			syslog(LOG_INFO, "Accepted connection from %s", ip4add);
			closelog();
			ssize_t recieved = recv(cfd, data, BUFFER_SIZE*sizeof(char), 0);//MSG_WAITALL
			shutdown(cfd, SHUT_RD);
			if (recieved>BUFFER_SIZE) {
				shutdown(cfd, SHUT_RDWR);
				continue;
			}
			printf("\nServer got %ld bytes of data: (%s)\n", recieved, data);
			appendtofile(&fd, data);
			ssize_t sent = appendFromFileToBuffAndSend(cfd, &fd, data);
			if(sent==-1) printf("Error %d (%s) when sending data to a client\n", errno, strerror(errno));
			printf("Server sent %ld bytes of data: (%s)\n", sent, data);
			data[0]='\0';
			shutdown(cfd, SHUT_RDWR);
			cfd=-1;
			printf("Closed connection from %s\n", ip4add);
			openlog(NULL, 0, LOG_USER);
			syslog(LOG_INFO, "Closed connection from %s", ip4add);
			closelog();
		}
	}
	closeAll(srvfd, cfd, fd);
	return rv;
}