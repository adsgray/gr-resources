#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/kdaemon.h>
#include "conf.h"
#include "rotate.h"

/*
       int  pthread_create(pthread_t  *  thread, pthread_attr_t *
       attr, void * (*start_routine)(void *), void * arg);
*/
/*
	a wrapper around pthread_create(3).
	it creates a "detached" thread -- when it exits,
	it goes totally away, does not hang around so that
	we can reap its exit code.
 */
static int
create_detached_thread(void * (*start_routine)(void *), void *arg)
{
	pthread_attr_t attrs;
	pthread_t dont_care;

	pthread_attr_init(&attrs);
	pthread_attr_setdetachstate(&attrs, 1);

	return pthread_create(&dont_care, &attrs, start_routine, arg);
}
