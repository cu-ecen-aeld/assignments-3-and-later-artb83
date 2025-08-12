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
	struct thread_data* thread_func_args = (struct thread_data*)thread_param;
	DEBUG_LOG("Thread started\n");

	usleep(thread_func_args->m_wait_to_obtain_ms);

	if(pthread_mutex_lock(thread_func_args->m_mutex)!=0){
		DEBUG_LOG("Could not obtain lock\n");
		return thread_param;
	}
	DEBUG_LOG("Thread critical section");

	thread_func_args->thread_complete_success=true;
	usleep(thread_func_args->m_wait_to_release_ms);
	pthread_mutex_unlock(thread_func_args->m_mutex);

	DEBUG_LOG("Thread complete\n");
    return thread_param;
}


bool start_thread_obtaining_mutex(pthread_t *thread, pthread_mutex_t *mutex,int wait_to_obtain_ms, int wait_to_release_ms)
{
	DEBUG_LOG("Thread starter function\n");
    /**
     * TODO: allocate memory for thread_data, setup mutex and wait arguments, pass thread_data to created thread
     * using threadfunc() as entry point.
     *
     * return true if successful.
     *
     * See implementation details in threading.h file comment block
     */
	bool bSuccess = true;
	struct thread_data* thread_func_args = (struct thread_data*)calloc(1, sizeof(struct thread_data));
	if(!thread_func_args){
		return false;
	}

	thread_func_args->thread_complete_success=false;
	thread_func_args->m_wait_to_obtain_ms=wait_to_obtain_ms;
	thread_func_args->m_wait_to_release_ms=wait_to_release_ms;
	thread_func_args->m_mutex=mutex;

	if(0!=pthread_create(thread, NULL, &threadfunc, thread_func_args)){
		ERROR_LOG("Failed to start a thread");
		free(thread_func_args);
		return false;
	}
	if(thread_func_args->thread_complete_success){
		bSuccess=true;
	}
    return bSuccess;
}
