#include "systemcalls.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>

/**
 * @param cmd the command to execute with system()
 * @return true if the command in @param cmd was executed
 *   successfully using the system() call, false if an error occurred,
 *   either in invocation of the system() call, or if a non-zero return
 *   value was returned by the command issued in @param cmd.
*/
bool do_system(const char *cmd)
{
	bool res = true;
/*
 * TODO  add your code here
 *  Call the system() function with the command set in the cmd
 *   and return a boolean true if the system() call completed with success
 *   or false() if it returned a failure
*/
	if(0!=system(cmd))res=false;

    return res;
}

/**
* @param count -The numbers of variables passed to the function. The variables are command to execute.
*   followed by arguments to pass to the command
*   Since exec() does not perform path expansion, the command to execute needs
*   to be an absolute path.
* @param ... - A list of 1 or more arguments after the @param count argument.
*   The first is always the full path to the command to execute with execv()
*   The remaining arguments are a list of arguments to pass to the command in execv()
* @return true if the command @param ... with arguments @param arguments were executed successfully
*   using the execv() call, false if an error occurred, either in invocation of the
*   fork, waitpid, or execv() command, or if a non-zero return value was returned
*   by the command issued in @param arguments with the specified arguments.
*/

bool do_exec(int count, ...)
{
    va_list args;
    va_start(args, count);
    char * command[count+1];
    int i;
    for(i=0; i<count; i++)
    {
        command[i] = va_arg(args, char *);
        printf("arg i=%d  %s\n", i, command[i]);
    }
    command[count] = NULL;
    // this line is to avoid a compile warning before your implementation is complete
    // and may be removed
    command[count] = command[count];
    va_end(args);

/*
 * TODO:
 *   Execute a system command by calling fork, execv(),
 *   and wait instead of system (see LSP page 161).
 *   Use the command[0] as the full path to the command to execute
 *   (first argument to execv), and use the remaining arguments
 *   as second argument to the execv() command.
 *
*/
    va_end(args);

    int fd;
    //Check full path for a command and a file that is tested
    if(-1==(fd=open(command[0], O_RDONLY, 0111)) || -1==(fd=open(command[count-1], O_RDONLY, 0111))) return false;
    close(fd);
    int rv = 0;
    pid_t cpid = fork();
    if(cpid<0) return false;
    if(cpid==0){
    	printf("command[0]=%s, command[count-1]=%s\n", command[0], command[count-1]);
		rv = execv(command[0], command);
    	printf("Child execv rv=%d \n", rv);
    }
    if(cpid>0){
    	rv = wait(NULL);
    }

    return (rv==-1 ? false : true);
}

/**
* @param outputfile - The full path to the file to write with command output.
*   This file will be closed at completion of the function call.
* All other parameters, see do_exec above
*/
bool do_exec_redirect(const char *outputfile, int count, ...)
{
    va_list args;
    va_start(args, count);
    char * command[count+1];
    int i;
    for(i=0; i<count; i++)
    {
        command[i] = va_arg(args, char *);
    }
    command[count] = NULL;
    // this line is to avoid a compile warning before your implementation is complete
    // and may be removed
    command[count] = command[count];


/*
 * TODO
 *   Call execv, but first using https://stackoverflow.com/a/13784315/1446624 as a refernce,
 *   redirect standard out to a file specified by outputfile.
 *   The rest of the behaviour is same as do_exec()
 *
*/
    va_end(args);

    int fd = open(outputfile, O_CREAT|O_RDWR, S_IRUSR|S_IWUSR|S_IRWXG|S_IRGRP);
    if(fd<0) return false;
    pid_t cpid;
    switch(cpid=fork()){
    case -1:
    	close(fd);
    	return false;
    case 0: //child process
    	if(0>dup2(fd,1)) return false;
    	int rv = execv(command[2], command);
    	if(rv==-1){
    		close(fd);
    		return false;
    	}
    default:
    	close(fd);
    	if(-1==wait(NULL))return false;
    }
    close(fd);
    return true;
}
