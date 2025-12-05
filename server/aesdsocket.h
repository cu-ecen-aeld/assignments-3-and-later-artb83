//
// Created by root on 9/17/25.
//

#ifndef SERVER_AESDSOCKET_H
#define SERVER_AESDSOCKET_H

#define LISTEN_BACKLOG 128
#define BUFFER_SIZE 10*1024*1024
#include<pthread.h>
#include <stdbool.h>
#include <sys/types.h>
#include "queue.h"

#define HEAD_SL head_sl
#define POLL_TIMEOUT_MSEC 100
#define TIMESTAMP_STRLEN 128
typedef struct {
    int clientFd;
    int* logFd;
    char* dataBuff;
    pthread_t threadId;
    pthread_mutex_t* mutex;
 	char ip4add[INET_ADDRSTRLEN]; //ipv4
    struct sockaddr_in* cInfo;
    bool threadComplete;
}thread_data_t;

typedef struct {
    int* logFd;
    pthread_t threadId;
    pthread_mutex_t* mutex;
    bool threadComplete;
}timestamp_thread_data_t;

struct threads_list_node_t{
    thread_data_t*       thrData;
    SLIST_ENTRY(threads_list_node_t) nodes;
};


void closeAll(int sfd, int cfd, int fd, struct pollfd* psrvfd);
void releaseThreadResourcesFromList(void);
static void signalHandler(int numOfSignal);
void writeMsgToSyslog(int log_facility, int log_priority, const char* msgToLog);
ssize_t appendtofile(int* fd, char* data);
ssize_t appendFromFileToBuffAndSend(int* cfd, int* fd, char* buff);
int daemonize(int srvfd);
int sigsubscribe(void* handler);

//threading
//init thread data struct
thread_data_t* allocAndInitThreadData(int clientFd, int* logFd, struct sockaddr_in* cInfo, pthread_mutex_t* mutex);


#endif //SERVER_AESDSOCKET_H