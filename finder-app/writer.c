#include <stdio.h>
#include <fcntl.h>
#include <syslog.h>
#include <unistd.h>
#include <string.h>


int main(int argc, const char** argv){
	openlog(NULL, 0, LOG_USER);
	if( argc!=3 ){
		syslog(LOG_ERR, "Invalid number of arguments: %d", argc);
		closelog();
		return 1;
	}
	const char* pathToFile = argv[1];
	const char* strToWrite = argv[2];
	int fd = open(pathToFile, O_CREAT|O_RDWR, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP );
	if(fd<0){
		syslog(LOG_ERR, "Could not open file: %s", pathToFile);
		closelog();
		return 1;
	}
	write(fd, strToWrite, strlen(strToWrite));
	syslog(LOG_DEBUG, "Writing %s to file %s", strToWrite, pathToFile);
	closelog();
	close(fd);
	return 0;
}
