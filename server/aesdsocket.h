//
// Created by root on 9/17/25.
//

#ifndef SERVER_AESDSOCKET_H
#define SERVER_AESDSOCKET_H

#define LISTEN_BACKLOG 50

void closeAll(int sfd, int cfd, int fd);
static void signalHandler(int numOfSignal);
void writeMsgToSyslog(int log_facility, int log_priority, const char* msgToLog);

#endif //SERVER_AESDSOCKET_H