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
#include <poll.h>
#include <time.h> //for timestamps
#include <sys/ioctl.h>
#include "aesdsocket.h"
#include "aesd_ioctl.h"

#include <linux/limits.h>

static volatile sig_atomic_t caught_sigint=false;
static volatile sig_atomic_t caught_sigterm=false;

static SLIST_HEAD(HEAD_SL, threads_list_node_t) head;

int listCount(void){
	int nEntries=0;
	struct threads_list_node_t* nodep=NULL;
	SLIST_FOREACH(nodep, &head, nodes) {
		nEntries++;
	}
	return nEntries;
}

void closeAll(int sfd, struct pollfd* psrvfd, pthread_mutex_t* mutex) {
	syslog(LOG_INFO, "aesdsocket exiting");
	shutdown(sfd, SHUT_RDWR);
	close(sfd);							//Stop accepting
	releaseThreadResourcesFromList();   //Unblock + join all worker threads, free pointers
	pthread_mutex_destroy(mutex);       //Destroy mutex
	if(psrvfd!=NULL) free(psrvfd);
#if !USE_AESD_CHAR_DEVICE
	remove(DATA_STORAGE_PATH);
#endif
	closelog();        //Close syslog
}

//Unblock + join worker threads, free linked list pointers
void releaseThreadResourcesFromList(void) {
	struct threads_list_node_t *nodep, *tmp;
	SLIST_FOREACH_SAFE(nodep, &head, nodes, tmp) {
		if (!atomic_load(&nodep->thrData->threadComplete))
			shutdown(nodep->thrData->clientFd, SHUT_RDWR);   // unblock a thread stuck in recv()
		pthread_join(nodep->thrData->threadId, NULL);
		SLIST_REMOVE(&head, nodep, threads_list_node_t, nodes);
		free(nodep->thrData->dataBuff);
		free(nodep->thrData);
		free(nodep);
	}
}

static void signalHandler(int numOfSignal){
	if( numOfSignal == SIGINT ) caught_sigint = true;
	if( numOfSignal == SIGTERM ) caught_sigterm = true;
}

bool isFdOpen(int* fd) {
	if( fcntl(*fd, F_GETFD) == -1 ) {
		if( errno == EBADF ) {
			return false; // The fd is closed/invalid
		}
	}
	return true; // The fd is open
}

ssize_t appendToStorage(int* fd, char* data) {
	char* dataId=NULL;
	ssize_t res = 0;
	bool isIoctl = false;
	size_t dataLen = strlen(data);
#if USE_AESD_CHAR_DEVICE
	uint32_t cmd = 0;
	uint32_t off = 0;
	dataId = strstr(data, "AESDCHAR_IOCSEEKTO");
	if ( NULL != dataId ) {
		isIoctl = true;
		syslog(LOG_INFO, "Found aesdchar_iocseekto command");
		//AESDCHAR_IOCSEEKTO:X,Y
		sscanf(dataId, "AESDCHAR_IOCSEEKTO:%u,%u", &cmd, &off);
	}
#endif
	if( NULL==dataId ) {
		dataId = strstr(data, "\n");
		if(dataId) dataLen = dataId - data + 1; //Including '\n' in dataLen
	}
	if( !isFdOpen(fd) ) *fd = open(DATA_STORAGE_PATH, O_CREAT|O_APPEND|O_RDWR, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP);
	if( 0<*fd ) {
		if( !isIoctl ){
			res=write(*fd, data, dataLen*sizeof(char));
			if( res<0 ) {
				syslog(LOG_INFO, "Append write to aesdchar storage error: %d | Message: %s", errno, strerror(errno));
			}
		}else {
			syslog(LOG_INFO, "AESDCHAR_IOCSEEKTO:%d,%d",cmd,off);
			struct aesd_seekto seekto;
			seekto.write_cmd = cmd;
			seekto.write_cmd_offset = off;
			res = ioctl(*fd, AESDCHAR_IOCSEEKTO, &seekto);
			if( res<0 ) {
				syslog(LOG_INFO, "Append ioctl to aesdchar storage error: %d | Message: %s", errno, strerror(errno));
			}
		}
		if( !isIoctl ) {
			close(*fd);
			*fd = -1;
		}
		return res;
	}
	return -1;
}

// Helper function - Send until buffer is completely sent - per assignment requirement.
// Retry sending if interrupted by a signal.
// Returns n-bytes sent or -1 on error.
static ssize_t sendUntilComplete(int cfd, const char* buff, size_t nSend) {
	size_t totalSent = 0;
	while (totalSent < nSend) {
		ssize_t sent=send(cfd, buff+totalSent, nSend-totalSent, MSG_NOSIGNAL);// MSG_NOSIGNAL - if client disconnects, send returns -1 instead of SIGPIPE.
		if (sent < 0) {
			if (errno == EINTR) continue;   // interrupted by signal, retry
			return -1;
		}
		totalSent += (size_t)sent;
	}
	return (ssize_t)totalSent;
}

ssize_t appendFromStorageToBuffAndSend(int* cfd, int* fd, char* buff) {
	ssize_t nRead = 0;
	ssize_t nSent = 0;
	ssize_t nReadTotal = 0;
	ssize_t nSentTotal = 0;

	if( !isFdOpen(fd) ) {
		if(0<*fd) syslog(LOG_INFO, "Reopening storage fd");
		*fd=open(DATA_STORAGE_PATH, O_RDONLY, S_IRUSR|S_IRGRP);
	}
	if( 0<*fd ){
		syslog(LOG_INFO, "Reading aesdchar storage");
		while( 0<(nRead = read(*fd, buff, BUFFER_SIZE)) ) {
			nReadTotal+=nRead;
			nSent = sendUntilComplete(*cfd, buff, nRead);
			if (nSent < 0) {
				syslog(LOG_ERR, "send error: %s", strerror(errno));
				break;
			}
			nSentTotal+=nSent;
		}
		if(nRead<0) {
			syslog(LOG_INFO, "Read aesdchar storage error code: %d | Message: %s", errno, strerror(errno));
		}else {
			syslog(LOG_INFO, "Read aesdchar storage data of %ld bytes, sent %ld bytes", nReadTotal, nSentTotal);
		}
	}
	shutdown(*cfd, SHUT_RDWR);
	close(*cfd);
	*cfd=-1;
	close(*fd);
	*fd = -1;
    return ( nRead<0 ? nRead : nSentTotal );
}

// Helper function - Receive until '\n', peer close, or buffer full - per assignment requirement.
// Returns bytes received (buffer is NUL-terminated), or -1 on error.
static ssize_t recvUntilNewline(int cfd, char* buff, size_t buffSize) {
	size_t total = 0;
	while (total < buffSize) {
		ssize_t n = recv(cfd, buff + total, (buffSize - total)*sizeof(char), 0);
		if (n < 0) {
			if (errno == EINTR) continue;   // interrupted by signal, retry
			return -1;
		}
		if (n == 0) break;                  // client closed the connection
		total += (size_t)n;
		if (memchr(buff + total - n, '\n', (size_t)n) != NULL) break;
	}
	buff[total] = '\0';                     // dataBuff is BUFFER_SIZE+1, so this is safe
	return (ssize_t)total;
}

void* rcvAndSndThread(void* thrArg) {
	thread_data_t* thrData = (thread_data_t*)thrArg;
	syslog(LOG_INFO, "Accepted connection from %s", thrData->ip4add);
	ssize_t received = recvUntilNewline(thrData->clientFd, thrData->dataBuff, BUFFER_SIZE);
	if(received<=0) {
		if (received<0) {
			syslog(LOG_ERR, "recv error from %s: %s", thrData->ip4add, strerror(errno));
		}
		shutdown(thrData->clientFd, SHUT_RDWR);
		close(thrData->clientFd);
		atomic_store(&thrData->threadComplete, true);
		return thrData;
	}
	shutdown(thrData->clientFd, SHUT_RD);
	pthread_mutex_lock(thrData->mutex);
	appendToStorage(thrData->storageFd, thrData->dataBuff);
	ssize_t	sent = appendFromStorageToBuffAndSend(&thrData->clientFd, thrData->storageFd, thrData->dataBuff);
	pthread_mutex_unlock(thrData->mutex);

	if(sent<0) syslog(LOG_ERR, "Error %d (%s) when sending data to a client", errno, strerror(errno));
	syslog(LOG_INFO, "Closed connection from %s", thrData->ip4add);
	atomic_store(&thrData->threadComplete, true);
	return thrData;
}

thread_data_t* allocAndInitThreadData(int clientFd, int* storageFd, struct sockaddr_in* cInfo, pthread_mutex_t* mutex) {
	//init thread data struct
	//allocate thread data struct
	thread_data_t* thd = calloc(1, sizeof(*thd));
	if (thd!=NULL) {
		thd->clientFd=clientFd;
		thd->storageFd=storageFd;
		thd->mutex=mutex;
		atomic_store(&thd->threadComplete, false);
		inet_ntop(AF_INET, &cInfo->sin_addr, thd->ip4add, INET_ADDRSTRLEN);
		// allocate data buffer
		thd->dataBuff=malloc((1+BUFFER_SIZE)*sizeof(*thd->dataBuff));
		if (thd->dataBuff==NULL) {
			free(thd);
			return NULL;
		}
	}
	return thd;
}

int daemonize(int srvfd){
	syslog(LOG_INFO, "Turning into a daemon");
	closelog();
	pid_t pid = fork();
	if(pid<0) {
		exit(EXIT_FAILURE);
	}else if (pid>0) {
		exit(EXIT_SUCCESS);//exit parent proc
	}

	if (setsid() ==-1) { //create new session and proc group
		int err = errno;
		openlog("aesdsocket", LOG_PID, LOG_USER);
		syslog(LOG_ERR, "setsid failed: %s", strerror(err));
		return -1;
	}

	pid = fork();
	if (pid<0) exit(EXIT_FAILURE);
	if (pid>0) exit(EXIT_SUCCESS); //exit parent proc
	//continue with child proc, daemon
	umask(0);
	if(chdir("/") != 0) return -1;
	for (int i=0; i<INR_OPEN_MAX; i++) {
		if (i==srvfd) continue; //inherit server socket descriptor
		close(i); //closing file descriptors, including stdin/out/err
	}
	//reopen stdin/out/err and redirect them to /dev/null
	open("/dev/null", O_RDWR);
	dup(0);
	dup(0);

	openlog("aesdsocket", LOG_PID, LOG_USER);   // reopen: gets a fresh, valid fd
	syslog(LOG_INFO, "Daemon started");
	return 0;
}

int sigsubscribe(void (*handler)(int)){
	struct sigaction new_action;

	memset(&new_action, 0, sizeof(struct sigaction));
	new_action.sa_handler = handler;
	if( sigaction(SIGINT, &new_action, NULL) !=0 ) {
		return -1;
	}
	if( sigaction(SIGTERM, &new_action, NULL) !=0 ) {
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
	int fd=-1;      //storage file descriptor for incomming stream message
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;
	//threading
	pthread_mutex_t mutex;
	SLIST_INIT(&head);
	openlog("aesdsocket", LOG_PID, LOG_USER); //Open syslog for writing
	syslog(LOG_INFO, "aesdsocket starting");
	if(0!=getaddrinfo(NULL, "9000", &hints, &servinfo)) {
		printf("Error %d (%s) when getting addrinfo\n", errno, strerror(errno));
		exit(EXIT_FAILURE);
	}
	psrvfd=calloc(1, sizeof(*psrvfd));
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
	if(servinfo!=NULL) freeaddrinfo(servinfo);
	if(rv<0) {
		printf("Error %d (%s) when binding a socket\n", errno, strerror(errno));
		exit(EXIT_FAILURE);
	}

	//Daemonize, subscribe to signals, listen for incoming connections and signals, continue running.
	if(bDaemon && daemonize(srvfd) != 0) exit(EXIT_FAILURE);
	if(sigsubscribe(signalHandler) != 0) {
		syslog(LOG_ERR, "Signal registration failed: %s", strerror(errno));
		exit(EXIT_FAILURE);
	}

	//listen, accept, connect and respond
	socklen_t cAddrLen=0;
	struct sockaddr_in cInfo;
	listen(srvfd, LISTEN_BACKLOG);

	//prepare for threading - init mutex
	pthread_mutex_init(&mutex, NULL);

	//Prepare for signal masking while in worker thread
	sigset_t workerBlockSet, oldSet;
	sigemptyset(&workerBlockSet);
	sigaddset(&workerBlockSet, SIGINT);
	sigaddset(&workerBlockSet, SIGTERM);

#if !USE_AESD_CHAR_DEVICE
	//init timers for timestamps
	char timeStampStr[TIMESTAMP_STRLEN] = {'\0'};
	struct timespec base;
	struct timespec timeStamp;
	clock_gettime(CLOCK_REALTIME, &base);
#endif
	//running the server
	while(bRun) {
		if(caught_sigint || caught_sigterm) {
			syslog(LOG_INFO, "Caught signal, exiting");
			closeAll(srvfd, psrvfd, &mutex);
			bRun=false;
			printf("\nCaught signal, exiting\n");
			exit(EXIT_SUCCESS);
		}
		poll(psrvfd, 1, POLL_TIMEOUT_MSEC);
#if !USE_AESD_CHAR_DEVICE
		clock_gettime(CLOCK_REALTIME, &timeStamp);
		if ((timeStamp.tv_sec-base.tv_sec)>=10) {
			/*
			 * strftime
			 * %F equivalent to %Y-%m-%d date format
			 * %T The time in 24-hour notation (%H:%M:%S).
			 * %n Next line char
			 */
			struct tm* curTime = localtime(&timeStamp.tv_sec);
			strftime(timeStampStr, TIMESTAMP_STRLEN, "timestamp:%F %T%n", curTime);
			pthread_mutex_lock(&mutex);
			fd = open(DATA_STORAGE_PATH, O_CREAT|O_APPEND|O_WRONLY, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP);
			write(fd,timeStampStr,strlen(timeStampStr));
			close(fd);
			pthread_mutex_unlock(&mutex);
			clock_gettime(CLOCK_REALTIME, &base);
		}
#endif
		cAddrLen=sizeof(cInfo);
		cfd = accept(srvfd, (struct sockaddr*)&cInfo, &cAddrLen);
		if (cfd < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) continue;
		if(cfd>=0){
			//allocate and init thread data struct
			thread_data_t* thd = allocAndInitThreadData(cfd, &fd, &cInfo, &mutex);
			//allocate threads list node - each new node represents a new worker thread
			struct threads_list_node_t* node = thd ? calloc(1, sizeof(*node)) : NULL;
			if (!node || !thd) {
				syslog(LOG_ERR, "Out of memory, dropping connection");
				close(cfd);
				if(thd) {
					free(thd->dataBuff);
					free(thd);
				}
				continue;
			}
			//init node
			node->thrData=thd;

			// mask signals, new thread inherits this mask, store current state into oldSet
			pthread_sigmask(SIG_BLOCK, &workerBlockSet, &oldSet);
			int rc = pthread_create(&thd->threadId, NULL, rcvAndSndThread, thd);
			// unmask signals, main unblocked again, restore from oldSet
			pthread_sigmask(SIG_SETMASK, &oldSet, NULL);

			if (rc != 0) {
				syslog(LOG_ERR, "pthread_create failed");
				close(cfd);
				free(thd->dataBuff); free(thd); free(node);
				continue;
			}
			// insert only after the thread exists
			SLIST_INSERT_HEAD(&head, node, nodes);
		}
		struct threads_list_node_t* nodep=NULL;
		struct threads_list_node_t* tmp=NULL;
		SLIST_FOREACH_SAFE(nodep, &head, nodes, tmp) {
			if (atomic_load(&nodep->thrData->threadComplete)) {
				pthread_join(nodep->thrData->threadId, NULL);
				SLIST_REMOVE(&head, nodep, threads_list_node_t, nodes);
				free(nodep->thrData->dataBuff);
				free(nodep->thrData);
				free(nodep);
			}
		}
	}
	closeAll(srvfd, psrvfd, &mutex);
	return rv;
}
