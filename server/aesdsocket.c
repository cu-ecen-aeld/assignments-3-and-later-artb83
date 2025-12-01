#include <errno.h>
#include <stdio.h>
#include <syslog.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netdb.h>
#include <arpa/inet.h> // for inet_ntop
#include <fcntl.h>
#include <linux/fs.h>
#include <sys/poll.h>
#include <time.h> //for timestamps
#include "aesdsocket.h"

#include <linux/limits.h>

static bool caught_sigint=false;
static bool caught_sigterm=false;
static SLIST_HEAD(HEAD_SL, threads_list_node_t) head;

int listCount(void){
	int nEntries=0;
	struct threads_list_node_t* nodep=NULL;
	SLIST_FOREACH(nodep, &head, nodes) {
		nEntries++;
	}
	return nEntries;
}

void closeAll(int sfd, int cfd, int fd, struct pollfd* psrvfd) {
	closelog();
	shutdown(sfd, SHUT_RDWR);
	close(sfd);
	close(cfd);
	close(fd);
	remove("/var/tmp/aesdsocketdata");
	if(psrvfd!=NULL) free(psrvfd);
	if(!SLIST_EMPTY(&head))releaseThreadResourcesFromList();
	// releaseThreadResourcesFromList();
}

//Very inefficient because of single linked list
void releaseThreadResourcesFromList(void) {
	printf("\nList release - threads list size %d nodes before release\n", listCount());
	struct threads_list_node_t* nodep=head.slh_first;
	struct threads_list_node_t* nextNodep=nodep;
	// printf("head.slh_first=%p\n", head.slh_first);

	while(nodep != NULL) {
		// printf("\nNode release\n");
		// printf("nodep=%p\n", nodep);
		nextNodep = nodep->nodes.sle_next;
		// printf("nextNodep=%p\n", nextNodep);
		pthread_join(nodep->thrData->threadId, NULL);
		pthread_mutex_unlock(nodep->thrData->mutex);
		pthread_mutex_destroy(nodep->thrData->mutex);
		close(nodep->thrData->clientFd);
		close(*nodep->thrData->logFd);
		if (nodep->thrData->dataBuff && nodep->thrData) free(nodep->thrData->dataBuff);
		if (nodep->thrData) {
			free(nodep->thrData);
			nodep->thrData = NULL;
		}
		// printf("nodep=%p before free\n", nodep);
		free(nodep);
		nodep=nextNodep;
		// printf("nodep=%p after free\n", nodep);
	}
	// printf("nodep=%p\n", nodep);
	printf("List release complete\n");
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

ssize_t appendtofile(int* fd, char* data) {
	char* newline=strstr(data, "\n");
	if( newline!=NULL && 0<(*fd = open("/var/tmp/aesdsocketdata", O_CREAT|O_APPEND|O_WRONLY, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP)) ){
		data[newline-data+1]='\0';//terminate string after \n character
		ssize_t res=write(*fd, data, (newline-data+1)*sizeof(char));
		close(*fd);
		return res;
	}
	return -1;
}
ssize_t appendFromFileToBuffAndSend(int* cfd, int* fd, char* buff) {
	int nRead = 0;
	ssize_t res=-1;
	if( 0<(*fd = open("/var/tmp/aesdsocketdata", O_RDONLY, S_IRUSR|S_IRGRP)) ){
		res=0;
		while( 0<(nRead = read(*fd, buff, BUFFER_SIZE)) ) {
			if( 0<(res=send(*cfd, buff, nRead, 0)) ) break; //MSG_FASTOPEN
		}
	}
	shutdown(*cfd, SHUT_RDWR);
	close(*cfd);
	*cfd=-1;
	close(*fd);
	return res;
}

void* rcvAndSndThread(void* thrArg) {
	thread_data_t* thrData = (thread_data_t*)thrArg;
	pthread_mutex_lock(thrData->mutex);
	openlog(NULL, 0, LOG_USER);
	syslog(LOG_INFO, "Accepted connection from %s", thrData->ip4add);
	closelog();
	ssize_t recieved = recv(thrData->clientFd, thrData->dataBuff, BUFFER_SIZE*sizeof(char), 0);//MSG_WAITALL
	shutdown(thrData->clientFd, SHUT_RD);
	if (recieved>BUFFER_SIZE) {
		shutdown(thrData->clientFd, SHUT_RDWR);
		close(thrData->clientFd);
		thrData->threadComplete = true;
		pthread_mutex_unlock(thrData->mutex);
		return thrData;
	}
	bool canstop=false;
	if(0==strcmp(thrData->dataBuff, "stop\n")) canstop=true;
	appendtofile(thrData->logFd, thrData->dataBuff);
	ssize_t sent=appendFromFileToBuffAndSend(&thrData->clientFd, thrData->logFd, thrData->dataBuff);
	if(sent==-1) printf("Error %d (%s) when sending data to a client\n", errno, strerror(errno));
	thrData->dataBuff[0]='\0';
	openlog(NULL, 0, LOG_USER);
	syslog(LOG_INFO, "Closed connection from %s", thrData->ip4add);
	closelog();
	thrData->threadComplete = true;
	pthread_mutex_unlock(thrData->mutex);
	if(canstop) kill(getpid(),SIGTERM);
	return thrData;
}

thread_data_t* allocAndInitThreadData(int clientFd, int* logFd, struct sockaddr_in* cInfo, pthread_mutex_t* mutex) {
	//init thread data struct
	//allocate thread data struct
	thread_data_t* thd = (thread_data_t*)calloc(1, sizeof(thread_data_t));
	if (thd!=NULL) {
		thd->clientFd=clientFd;
		thd->logFd=logFd;
		thd->mutex=mutex;
		thd->threadComplete=false;
		inet_ntop(AF_INET, &cInfo->sin_addr, thd->ip4add, INET_ADDRSTRLEN);
		// allocate data buffer
		thd->dataBuff=(char*)malloc((1+BUFFER_SIZE)*sizeof(char));
		if (thd->dataBuff==NULL) {
			free(thd);
			return NULL;
		}
	}
	return thd;
}

int daemonize(int srvfd){
	pid_t pid = fork();
	if(pid<0) {
		exit(EXIT_FAILURE);
	}else if (pid>0) {
		exit(EXIT_SUCCESS);//exit parent proc
	}

	if (setsid() ==-1) return -1; //create new session and proc group

	pid = fork();
	if (pid<0) exit(EXIT_FAILURE);
	if (pid>0) exit(EXIT_SUCCESS); //exit parent proc
	//continue with child proc, daemon
	umask(0);
	chdir("/");
	for (int i=0; i<INR_OPEN_MAX; i++) {
		if (i==srvfd) continue; //inherit server socket descriptor
		close(i); //closing file descriptors, including stdin/out/err
	}
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
	bool bDaemon=(argc>1 ? (strcmp(argv[1], "-d")==0 || strcmp(argv[1], "d")==0) : false);
	bool bRun=true;
	struct addrinfo hints;
	struct addrinfo* servinfo=NULL;
	struct pollfd* psrvfd=NULL;
	int srvfd = 0; //server
	int cfd = 0;   //client
	int fd=0;      //file for incomming stream message
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;
	//threading
	pthread_mutex_t mutex;
	SLIST_INIT(&head);
	if(0!=getaddrinfo(NULL, "9000", &hints, &servinfo)) {
		printf("Error %d (%s) when getting addrinfo\n", errno, strerror(errno));
		exit(EXIT_FAILURE);
	}
	psrvfd=(struct pollfd*)calloc(1, sizeof(struct pollfd));
	if(psrvfd==NULL) {
		printf("Error %d (%s) when creating struct pollfd*\n", errno, strerror(errno));
		exit(EXIT_FAILURE);
	}

	srvfd=socket(servinfo->ai_family, (servinfo->ai_socktype | SOCK_NONBLOCK), servinfo->ai_protocol);
	if(srvfd<0) {
		printf("Error %d (%s) when creating a socket\n", errno, strerror(errno));
		exit(EXIT_FAILURE);
	}
	//setup a server's non-blocking socket: polling, reuse address and bind
	psrvfd->fd = srvfd;
	psrvfd->events|=POLLIN;
	int yes=1;
	int rv=0;
	setsockopt(srvfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));
	rv=bind(srvfd, servinfo->ai_addr, servinfo->ai_addrlen);
	if(servinfo!=NULL)freeaddrinfo(servinfo);
	if( rv<0 ) {
		printf("Error %d (%s) when binding a socket\n", errno, strerror(errno));
		exit(EXIT_FAILURE);
	}

	if(!bRun) { //if any errors, close all and exit
		closeAll(srvfd, cfd, fd, psrvfd);
		exit(EXIT_FAILURE);
	} else {    //else subscribe to signals, listen for incoming connections and signals, continue running.
		if(bDaemon) bRun = (daemonize(srvfd) == 0 ? true : false);
		bRun = (0==sigsubscribe(signalHandler) ? true : false);
	}


	//listen, accept, connect and respond
	socklen_t cAddrLen=0;
	struct sockaddr_in cInfo;

	//listen
	//prepare for threading - init mutex
	if(bRun) {
		listen(srvfd, LISTEN_BACKLOG);
		pthread_mutex_init(&mutex, NULL);
	}

	struct timespec base;
	struct timespec timeStamp;
	clock_gettime(CLOCK_MONOTONIC, &base);
	__time_t dif=0;
	//running the server
	while(bRun) {
		if(caught_sigint || caught_sigterm) {
			writeMsgToSyslog(LOG_USER, LOG_INFO, "Caught signal, exiting");
			closeAll(srvfd, cfd, fd, psrvfd);
			bRun=false;
			printf("\nCaught signal, exiting\n");
			exit(EXIT_SUCCESS);
		}
		int ready=poll(psrvfd, 1, POLL_TIMEOUT_MSEC);
		clock_gettime(CLOCK_MONOTONIC, &timeStamp);
		if ((dif=(timeStamp.tv_sec-base.tv_sec))>=10) {
			clock_gettime(CLOCK_MONOTONIC, &base);
			printf("TimeStamp=%ld - ten\n", dif);
		}
		if (ready>0) printf("Ready=%d - accept\n", ready);
		cAddrLen=sizeof(cInfo);
		cfd = accept(srvfd, (struct sockaddr*)&cInfo, &cAddrLen);
		if (cfd==EAGAIN) continue;
		if(cfd>0){
			//allocate and init thread data struct
			thread_data_t* thd = allocAndInitThreadData(cfd, &fd, &cInfo, &mutex);
			//allocate threads list node
			struct threads_list_node_t* node = (struct threads_list_node_t*)calloc(1, sizeof(struct threads_list_node_t));
			// printf("New list node p=%p\n", node);
			//init node
			node->thrData=thd;
			//insert node into the list
			SLIST_INSERT_HEAD(&head, node, nodes);
			pthread_create(&thd->threadId, NULL, rcvAndSndThread, thd);
		}
		struct threads_list_node_t* nodep=NULL;
		SLIST_FOREACH_SAFE(nodep, &head, nodes, nodep->nodes.sle_next) {
			if (nodep->thrData->dataBuff!=NULL && nodep->thrData->threadComplete) {
				printf("Threads list size %d nodes\n", listCount());
				// printf("Release data buffer of complete thread\n");
				pthread_join(nodep->thrData->threadId, NULL);
				if(nodep->thrData->dataBuff) {
					free(nodep->thrData->dataBuff);
					nodep->thrData->dataBuff=NULL;
				}
				// printf("Data release complete\n");
			}
		}
	}
	closeAll(srvfd, cfd, fd, psrvfd);
	return rv;
}