//
// Created by root on 9/17/25.
//

#ifndef SERVER_AESDSOCKET_H
#define SERVER_AESDSOCKET_H

#define LISTEN_BACKLOG 50
#define BUFFER_SIZE 10*1024*1024

void closeAll(int sfd, int cfd, int fd);
static void signalHandler(int numOfSignal);
void writeMsgToSyslog(int log_facility, int log_priority, const char* msgToLog);
int appendtofile(int* fd, char* data);
ssize_t appendFromFileToBuffAndSend(int* cfd, int* fd, char* buff);
int daemonize(int srvfd);
int sigsubscribe(void* handler);
#endif //SERVER_AESDSOCKET_H