/******************************************************
A fast mutex for interprocess synchronization.
mutex_t can be used only within single process,
but ip_mutex_t also between processes.

(c) 1995 Innobase Oy

Created 9/30/1995 Heikki Tuuri
*******************************************************/
#include "sync0ipm.h"
#ifdef UNIV_NONINL
#include "sync0ipm.ic"
#endif

#include "mem0mem.h"

/* The performance of the ip mutex in NT depends on how often
a thread has to suspend itself waiting for the ip mutex
to become free. The following variable counts system calls
involved. */

ulint	ip_mutex_system_call_count	= 0;

/**********************************************************************
Creates, or rather, initializes
an ip mutex object in a specified shared memory location (which must be
appropriately aligned). The ip mutex is initialized in the reset state.
NOTE! Explicit destroying of the ip mutex with ip_mutex_free
is not recommended
as the mutex resides in shared memory and we cannot make sure that
no process is currently accessing it. Therefore just use
ip_mutex_close to free the operating system event and mutex. */

ulint
ip_mutex_create(
/*============*/
					/* out: 0 if succeed */
	ip_mutex_t*	ip_mutex,	/* in: pointer to shared memory */
	char*		name,		/* in: name of the ip mutex */
	ip_mutex_hdl_t** handle)	/* out, own: handle to the
					created mutex; handle exists
					in the private address space of
					the calling process */
{
	mutex_t*	mutex;
	char*		buf;
	os_event_t	released;
	os_mutex_t	exclude;
	
	ip_mutex_set_waiters(ip_mutex, 0);

	buf = mem_alloc(strlen(name) + 20);

	strcpy(buf, name);
	strcpy(buf + strlen(name), "_IB_RELS");

	released = os_event_create(buf);

	if (released == NULL) {
		mem_free(buf);
		return(1);
	}

	strcpy(buf + strlen(name), "_IB_EXCL");

	exclude = os_mutex_create(buf);

	if (exclude == NULL) {
		os_event_free(released);
		mem_free(buf);
		return(1);
	}

	mutex = ip_mutex_get_mutex(ip_mutex);

	mutex_create(mutex);
	mutex_set_level(mutex, SYNC_NO_ORDER_CHECK);
	
	*handle = mem_alloc(sizeof(ip_mutex_hdl_t));

	(*handle)->ip_mutex = ip_mutex;
	(*handle)->released = released;
	(*handle)->exclude = exclude;

	mem_free(buf);

	return(0);
}

/**********************************************************************
NOTE! Using this function is not recommended. See the note
on ip_mutex_create. Destroys an ip mutex */

void
ip_mutex_free(
/*==========*/
	ip_mutex_hdl_t*	handle)		/* in, own: ip mutex handle */
{
	mutex_free(ip_mutex_get_mutex(handle->ip_mutex));

	os_event_free(handle->released);
	os_mutex_free(handle->exclude);

	mem_free(handle);
}

/**********************************************************************
Opens an ip mutex object in a specified shared memory location.
Explicit closing of the ip mutex with ip_mutex_close is necessary to
free the operating system event and mutex created, and the handle. */

ulint
ip_mutex_open(
/*==========*/
					/* out: 0 if succeed */
	ip_mutex_t*	ip_mutex,	/* in: pointer to shared memory */
	char*		name,		/* in: name of the ip mutex */
	ip_mutex_hdl_t** handle)	/* out, own: handle to the
					opened mutex */
{
	char*		buf;
	os_event_t	released;
	os_mutex_t	exclude;
	
	buf = mem_alloc(strlen(name) + 20);

	strcpy(buf, name);
	strcpy(buf + strlen(name), "_IB_RELS");

	released = os_event_create(buf);

	if (released == NULL) {
		mem_free(buf);
		return(1);
	}

	strcpy(buf + strlen(name), "_IB_EXCL");

	exclude = os_mutex_create(buf);

	if (exclude == NULL) {
		os_event_free(released);
		mem_free(buf);
		return(1);
	}

	*handle = mem_alloc(sizeof(ip_mutex_hdl_t));

	(*handle)->ip_mutex = ip_mutex;
	(*handle)->released = released;
	(*handle)->exclude = exclude;

	mem_free(buf);

	return(0);
}

/**********************************************************************
Closes an ip mutex. */

void
ip_mutex_close(
/*===========*/
	ip_mutex_hdl_t*	handle)		/* in, own: ip mutex handle */
{
	os_event_free(handle->released);
	os_mutex_free(handle->exclude);

	mem_free(handle);
}
