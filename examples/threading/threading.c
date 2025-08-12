#include "threading.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

// Optional: use these functions to add debug or error prints to your application
#define DEBUG_LOG(msg,...)
//#define DEBUG_LOG(msg,...) printf("threading: " msg "\n" , ##__VA_ARGS__)
#define ERROR_LOG(msg,...) printf("threading ERROR: " msg "\n" , ##__VA_ARGS__)

void* threadfunc(void* thread_param)
{
    // TODO: wait, obtain mutex, wait, release mutex as described by thread_data structure
    // hint: use a cast like the one below to obtain thread arguments from your parameter
	DEBUG_LOG("\nInside thread function\n");
	struct thread_data* thread_func_args = (struct thread_data*)thread_param;

	usleep(thread_func_args->m_wait_to_obtain_ms);
	if(pthread_mutex_lock(thread_func_args->m_mutex)!=0) return thread_param;

	usleep(thread_func_args->m_wait_to_release_ms);
	if(pthread_mutex_unlock(thread_func_args->m_mutex)!=0) return thread_param;

	thread_func_args->thread_complete_success=true;
	DEBUG_LOG("Thread function complete");
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

	if(0!=pthread_create(thread, NULL, &threadfunc, thread_func_args)){
		ERROR_LOG("Failed to start a thread");
		free(thread_func_args);
		return false;
	}
//	if(!thread_func_args->thread_complete_success){
//		pthread_detach(*thread);
//	} else{
//		void* res;
//		pthread_join(*thread, &res);
//		//printf("Joined with thread %ld; returned value was %c\n", *thread, *(char *)res);
//	}
	bSuccess = thread_func_args->thread_complete_success;
	if(bSuccess) DEBUG_LOG("Threading OK");
	free(thread_func_args);
    return bSuccess;
}

//int main(){
//	printf("Main Started\n");
//	bool thread_complete_success=false;
//	int wait_to_obtain_ms=1000;
//	int wait_to_release_ms=1000;
//	struct thread_data thread_func_args;
//	pthread_mutex_t mutex;
//	pthread_t thread;
//	thread_func_args.m_wait_to_obtain_ms=wait_to_obtain_ms;
//	thread_func_args.m_wait_to_release_ms=wait_to_release_ms;
//	thread_func_args.thread_complete_success = false;
//	pthread_mutex_init(&mutex, NULL);
//	if(0!=pthread_create(&thread, NULL, &threadfunc, &thread_func_args)){
//			ERROR_LOG("Failed to start a thread");
//			return false;
//	}
//	//thread_complete_success=start_thread_obtaining_mutex(&thread, &mutex, wait_to_obtain_ms, wait_to_release_ms);
//	pthread_join(thread, NULL);
//	if(thread_complete_success) DEBUG_LOG("Main OK");
//	else ERROR_LOG("Main Failed");
//	pthread_mutex_destroy(&mutex);
//	return 0;
//}
