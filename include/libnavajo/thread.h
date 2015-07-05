//********************************************************
/**
 * @file  thread.h
 *
 * @brief thread's facilities
 *
 * @author T.Descombes (thierry.descombes@gmail.com)
 *
 * @version 1        
 * @date 19/02/15
 */
//********************************************************

#ifndef THREAD_H_
#define THREAD_H_

#include <stdio.h>
#include <string.h>

extern "C"
{
  #include "pthread.h"
}

#define STRERROR_BUF    char strerror_buf[256]

#ifdef WIN32
#define STRERROR(en)  strerror_s(strerror_buf, sizeof strerror_buf, en)

#else

#define STRERROR(en) (                                                      \
    strerror_r(en, strerror_buf, sizeof strerror_buf) == 0                  \
        ? strerror_buf : "Unknown error"                                    \
)   

#endif



/***********************************************************************/

inline void create_thread(pthread_t *thread_p,
	 	void *(* thread_rtn)(void *), void *data_p)
{
	pthread_attr_t  thread_attr;	    /* Thread attributes		 */
	int		rc;		    /* Return code (error number)	 */
	STRERROR_BUF;			    /* Buffer for strerror_r()  	 */


	rc = pthread_attr_init(&thread_attr);
	if (rc != 0)
	    fprintf(stderr, "pthread_attr_init(): %s\n", STRERROR(rc));

	rc = pthread_attr_setdetachstate(&thread_attr, PTHREAD_CREATE_JOINABLE);
	if (rc != 0)
	    fprintf(stderr, "pthread_attr_setdetachstate(): %s\n", STRERROR(rc));
	    
	rc = pthread_attr_setstacksize (&thread_attr, 512 * 1024); // par défault 2*1024*1024
	if (rc != 0)
	    fprintf(stderr, "pthread_attr_setstacksize(): %s\n", STRERROR(rc));
		
	rc = pthread_create(thread_p, &thread_attr, thread_rtn, data_p);
	if (rc != 0)
	    fprintf(stderr, "pthread_create(): %s\n", STRERROR(rc));
	
	rc = pthread_attr_destroy(&thread_attr);
	if (rc != 0)
	    fprintf(stderr, "pthread_attr_destroy(): %s\n", STRERROR(rc));

	return;
}

/***********************************************************************/
/*
*  The cancel_thread() function requests the cancellation of the specified
*  thread.
*/
 
inline void cancel_thread(pthread_t thread)
{
	int		rc;		    /* Return code (error number)	 */
	STRERROR_BUF;			    /* Buffer for strerror_r()  	 */


	rc = pthread_cancel(thread);

	if (rc != 0)
	    fprintf(stderr, "pthread_cancel(): %s\n", STRERROR(rc));

	return;
}
 
/***********************************************************************/
/*
 *  The wait_for_thread() function waits for the specified thread to
 *  terminate.
 */
 
inline void wait_for_thread(pthread_t thread)
{
    int             rc;                 /* Return code (error number)        */
    STRERROR_BUF;                       /* Buffer for strerror_r()           */
 
    rc = pthread_join(thread, NULL);
    if (rc != 0)
        fprintf(stderr, "pthread_join(): %s\n", STRERROR(rc));
 
    return;
}

/***********************************************************************/

inline void cancelstate_thread(void)
{
	  int             rc;                 /* Return code (error number)        */
    	  STRERROR_BUF;                       /* Buffer for strerror_r()           */
 
	  rc=pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
	  if (rc != 0) 
        	fprintf(stderr, "pthread_setcancelstate(): %s\n", STRERROR(rc));

	  rc=pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED,NULL);
	  if (rc != 0) 
        	fprintf(stderr, "pthread_setcanceltype(): %s\n", STRERROR(rc));
}

/***********************************************************************/

struct cancelArg
{
	pthread_t *p;  // pthread to kill
	unsigned  s;   // second
	unsigned  ns;  // nanosecond
};

#ifndef WIN32
static void *thread_timeout_scheduler(void *arg)
{
	cancelstate_thread();
	
	cancelArg *ca=(cancelArg*)arg;
	pthread_t thread_to_kill=*(ca->p);

	timespec t1={ca->s, ca->ns};
	timespec t2;

	if (nanosleep(&t1,&t2)==-1)
	      pthread_exit(0);

	if (t2.tv_nsec>0)
	      pthread_exit(0);

	cancel_thread(thread_to_kill);
	wait_for_thread(thread_to_kill);
	printf("Timeout Exceeded: The thread as been cancelled !!! \n");

	pthread_exit(0);
	return NULL;
};

#endif

#endif
