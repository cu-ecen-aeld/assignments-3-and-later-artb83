#include "threading.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

// Optional: use these functions to add debug or error prints to your application
//#define DEBUG_LOG(msg,...)
#define DEBUG_LOG(msg,...) printf("threading: " msg "\n" , ##__VA_ARGS__)
#define ERROR_LOG(msg,...) printf("threading ERROR: " msg "\n" , ##__VA_ARGS__)

void* threadfunc(void* thread_param)
{
    // TODO: wait, obtain mutex, wait, release mutex as described by thread_data structure
    // hint: use a cast like the one below to obtain thread arguments from your parameter
    //struct thread_data* thread_func_args = (struct thread_data *) thread_param;
	DEBUG_LOG("Inside thread function\n");
	struct thread_data* thread_func_args = (struct thread_data*) thread_param;
	usleep(thread_func_args->m_wait_to_obtain_ms);
	//DEBUG_LOG("Trying to obtain mutex\n");
	int rc = pthread_mutex_lock(thread_func_args->m_mutex);
	if(rc!=0){
		//DEBUG_LOG("Could not obtain mutex\n");
		return thread_param;
	}
	//DEBUG_LOG("Critical section, obtained mutex waiting for release\n");
	usleep(1000*thread_func_args->m_wait_to_release_ms);
	pthread_mutex_unlock(thread_func_args->m_mutex);

	thread_func_args->thread_complete_success=true;
    return thread_param;
}


bool start_thread_obtaining_mutex(pthread_t *thread, pthread_mutex_t *mutex,int wait_to_obtain_ms, int wait_to_release_ms)
{
	DEBUG_LOG("Inside thread starter function: start_thread_obtaining_mutex\n");
    /**
     * TODO: allocate memory for thread_data, setup mutex and wait arguments, pass thread_data to created thread
     * using threadfunc() as entry point.
     *
     * return true if successful.
     *
     * See implementation details in threading.h file comment block
     */
	bool bSuccess = true;
	struct thread_data* thread_func_args = (struct thread_data*)malloc(sizeof(struct thread_data));
	thread_func_args->thread_complete_success=false;
	thread_func_args->m_wait_to_obtain_ms=wait_to_obtain_ms;
	thread_func_args->m_wait_to_release_ms=wait_to_release_ms;
	thread_func_args->m_mutex=mutex;
	int rc = pthread_create(thread, NULL, threadfunc, thread_func_args);
	DEBUG_LOG("pthread_create rc=%d", rc);
	if(rc!=0){
		ERROR_LOG("Failed to start a thread, err=%d", rc);
		free(thread_func_args);
		return false;
	}
	if(!thread_func_args->thread_complete_success){
		pthread_detach(*thread);
	} else{
		void* res;
		pthread_join(*thread, &res);
		printf("Joined with thread %ld; returned value was %c\n", *thread, *(char *)res);
	}
	bSuccess = thread_func_args->thread_complete_success;
	if(bSuccess) DEBUG_LOG("Threading OK");
	else DEBUG_LOG("Threading FAILED");
	free(thread_func_args);
    return bSuccess;
}

//int main(){
//	return 0;
//}
