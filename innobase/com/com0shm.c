/******************************************************
The communication through shared memory

(c) 1995 Innobase Oy

Created 9/25/1995 Heikki Tuuri
*******************************************************/

#include "com0shm.h"
#ifdef UNIV_NONINL
#include "com0shm.ic"
#endif

#include "mem0mem.h"
#include "ut0mem.h"
#include "com0com.h"
#include "os0shm.h"
#include "sync0sync.h"
#include "sync0ipm.h"
#include "hash0hash.h"

/*
	IMPLEMENTATION OF COMMUNICATION PRIMITIVES
	==========================================

When bind is called for an endpoint, a shared memory area of
a size specified by com_shm_set_option is created with the
name of the address given concatenated to "_IBSHM".
Also a mutex is created for controlling the access to the
shared memory area. The name of the mutex is address + "_IBSHM_MTX".
An event with name address + "_IBSHM_EV_NE" is created. This event
is in signaled state when the shared memory area is not empty, i.e.,
there is a datagram to read. An event address + "_IBSHM_EV_EM"
is signaled, when the area is empty, i.e., a datagram can be
written to it.

The shared memory area consists of an info struct
at the beginning, containing fields telling:
if the area is valid, i.e., is anybody going to
read it, whether it currently contains a datagram, the
length of the address from which the datagram was received,
the length of the datagram, and the maximum allowed length of
a datagram.
After the info struct, there is a string of bytes
containing the sender address and the data
of the datagram.
*/

/* The following is set TRUE when the first endpoint is created. */

ibool	com_shm_initialized	= FALSE;

/* When a datagram is sent, the shared memory area
corresponding to the destination address is mapped
to the address space of this (sender) process.
We preserve it and keep the relevant info in the
following list. We can save a lot of CPU time
if the destination can be found on the list. The list is
protected by the mutex below. */

mutex_t			com_shm_destination_mutex;
hash_table_t*		com_shm_destination_cache;
UT_LIST_BASE_NODE_T(com_shm_endpoint_t)
			com_shm_destination_list;

/* The number of successfully bound endpoints in this process. When this
number drops to 0, the destination cache is freed. This variable is protected
by com_shm_destination_mutex above. */

ulint			com_shm_bind_count	= 0;

/* The performance of communication in NT depends on how
many times a system call is made (excluding os_thread_yield,
as that is the fastest way to switch thread).
The following variable counts such events. */

ulint	com_shm_system_call_count = 0;

/* The info struct at the beginning of a shared memory area */

typedef struct com_shm_info_struct	com_shm_info_t;

/* An area of shared memory consists of an info struct followed
by a string of bytes. */

typedef com_shm_info_t	com_shm_t;

struct com_shm_endpoint_struct{
	ibool		owns_shm; /* This is true if the shared memory
				area is owned by this endpoint structure
				(it may also be opened for this endpoint,
				not created, in which case does not own it) */
	char*		addr;	/* pointer to address the endpoint is bound
				to, NULL if not bound */
	ulint		addr_len; /* address length */
	ulint		size;	/* maximum allowed datagram size, initialized
				to 0 at creation */
	os_shm_t	shm;	/* operating system handle of the shared
				memory area */
	com_shm_t*	map;	/* pointer to the start address of the shared
				memory area */
	os_event_t	not_empty; /* this is in the signaled state if
				the area currently may contain a datagram;
				NOTE: automatic event */
	os_event_t	empty; 	/* this is in the signaled state if the area
				currently may be empty; NOTE: automatic event */
	ip_mutex_hdl_t*	ip_mutex; /* handle to the interprocess mutex
				protecting the shared memory */
	UT_LIST_NODE_T(com_shm_endpoint_t) list; /* If the endpoint struct
				is inserted into a list, this contains
				pointers to next and prev */
	com_shm_endpoint_t* addr_hash;
				/* hash table link */
};

struct com_shm_info_struct{
	ulint		valid;	/* This is COM_SHM_VALID if the creator
				of the shared memory area has it still
				mapped to its address space. Otherwise,
				we may conclude that the datagram cannot
				be delivered. */
	ibool		not_empty; /* TRUE if the area currently contains
				a datagram */
	ulint		empty_waiters; /* Count of (writer) threads which are
				waiting for the empty-event */
	ulint		max_len;/* maximum allowed length for a datagram */
	ulint		addr_len;/* address length for the sender address */
	ulint		data_len;/* datagram length */
	ip_mutex_t	ip_mutex;/* fast interprocess mutex protecting
				the shared memory area */
};

#define COM_SHM_VALID	76640

/*************************************************************************
Accessor functions for a shared memory endpoint */

UNIV_INLINE
ibool
com_shm_endpoint_get_owns_shm(
/*==========================*/
	com_shm_endpoint_t*	ep)
{
	ut_ad(ep);
	return(ep->owns_shm);
}

UNIV_INLINE
void
com_shm_endpoint_set_owns_shm(
/*==========================*/
	com_shm_endpoint_t*	ep,
	ibool			flag)
{
	ut_ad(ep);

	ep->owns_shm = flag;
}

UNIV_INLINE
char*
com_shm_endpoint_get_addr(
/*======================*/
	com_shm_endpoint_t*	ep)
{
	ut_ad(ep);
	return(ep->addr);
}

UNIV_INLINE
void
com_shm_endpoint_set_addr(
/*======================*/
	com_shm_endpoint_t*	ep,
	char*			addr)
{
	ut_ad(ep);

	ep->addr = addr;
}

UNIV_INLINE
ulint
com_shm_endpoint_get_addr_len(
/*==========================*/
	com_shm_endpoint_t*	ep)
{
	return(ep->addr_len);
}

UNIV_INLINE
void
com_shm_endpoint_set_addr_len(
/*==========================*/
	com_shm_endpoint_t*	ep,
	ulint			len)
{
	ut_ad(ep);
	ut_ad(len > 0);

	ep->addr_len = len;
}

ulint
com_shm_endpoint_get_size(
/*======================*/
	com_shm_endpoint_t*	ep)
{
	return(ep->size);
}

UNIV_INLINE
void
com_shm_endpoint_set_size(
/*======================*/
	com_shm_endpoint_t*	ep,
	ulint			size)
{
	ut_ad(ep);

	ep->size = size;
}

UNIV_INLINE
os_shm_t
com_shm_endpoint_get_shm(
/*=====================*/
	com_shm_endpoint_t*	ep)
{
	return(ep->shm);
}

UNIV_INLINE
void
com_shm_endpoint_set_shm(
/*=====================*/
	com_shm_endpoint_t*	ep,
	os_shm_t		shm)
{
	ut_ad(ep);
	ut_ad(shm);

	ep->shm = shm;
}

UNIV_INLINE
com_shm_t*
com_shm_endpoint_get_map(
/*=====================*/
	com_shm_endpoint_t*	ep)
{
	return(ep->map);
}

UNIV_INLINE
void
com_shm_endpoint_set_map(
/*=====================*/
	com_shm_endpoint_t*	ep,
	com_shm_t*		map)
{
	ut_ad(ep);
	ut_ad(map);

	ep->map = map;
}

UNIV_INLINE
os_event_t
com_shm_endpoint_get_empty(
/*=======================*/
	com_shm_endpoint_t*	ep)
{
	return(ep->empty);
}

UNIV_INLINE
void
com_shm_endpoint_set_empty(
/*=======================*/
	com_shm_endpoint_t*	ep,
	os_event_t		event)
{
	ut_ad(ep);
	ut_ad(event);

	ep->empty = event;
}

UNIV_INLINE
os_event_t
com_shm_endpoint_get_not_empty(
/*===========================*/
	com_shm_endpoint_t*	ep)
{
	return(ep->not_empty);
}

UNIV_INLINE
void
com_shm_endpoint_set_not_empty(
/*===========================*/
	com_shm_endpoint_t*	ep,
	os_event_t		event)
{
	ut_ad(ep);
	ut_ad(event);

	ep->not_empty = event;
}

/************************************************************************
Accessor functions for the shared memory area info struct. */
UNIV_INLINE
ulint
com_shm_get_valid(
/*==============*/
	com_shm_info_t*	info)
{
	return(info->valid);
}

UNIV_INLINE
void
com_shm_set_valid(
/*==============*/
	com_shm_info_t*		info,
	ulint			flag)
{
	ut_ad(info);

	info->valid = flag;
}

UNIV_INLINE
ibool
com_shm_get_not_empty(
/*==================*/
	com_shm_info_t*	info)
{
	return(info->not_empty);
}

UNIV_INLINE
void
com_shm_set_not_empty(
/*==================*/
	com_shm_info_t*		info,
	ibool			flag)
{
	ut_ad(info);

	info->not_empty = flag;
}

UNIV_INLINE
ulint
com_shm_get_empty_waiters(
/*======================*/
	com_shm_info_t*	info)
{
	ut_ad(info->empty_waiters < 1000);

	return(info->empty_waiters);
}

UNIV_INLINE
void
com_shm_set_empty_waiters(
/*======================*/
	com_shm_info_t*	info,
	ulint		count)
{
	ut_ad(info);
	ut_ad(count < 1000);

	info->empty_waiters = count;
}

UNIV_INLINE
ulint
com_shm_get_max_len(
/*================*/
	com_shm_info_t*	info)
{
	return(info->max_len);
}

UNIV_INLINE
void
com_shm_set_max_len(
/*================*/
	com_shm_info_t*		info,
	ulint			len)
{
	ut_ad(info);
	ut_ad(len > 0);

	info->max_len = len;
}

UNIV_INLINE
ulint
com_shm_get_addr_len(
/*=================*/
	com_shm_info_t*	info)
{
	return(info->addr_len);
}

UNIV_INLINE
void
com_shm_set_addr_len(
/*=================*/
	com_shm_info_t*		info,
	ulint			len)
{
	ut_ad(info);
	ut_ad(len > 0);

	info->addr_len = len;
}

UNIV_INLINE
ulint
com_shm_get_data_len(
/*=================*/
	com_shm_info_t*	info)
{
	return(info->data_len);
}

UNIV_INLINE
void
com_shm_set_data_len(
/*=================*/
	com_shm_info_t*		info,
	ulint			len)
{
	ut_ad(info);
	ut_ad(len > 0);

	info->data_len = len;
}

UNIV_INLINE
ip_mutex_t*
com_shm_get_ip_mutex(
/*=================*/
	com_shm_info_t*	info)
{
	return(&(info->ip_mutex));
}

/*************************************************************************
Accessor functions for the address and datagram fields inside a
shared memory area. */

UNIV_INLINE
char*
com_shm_get_addr(
/*=============*/
	com_shm_t*	area)
{
	return((char*)area + sizeof(com_shm_info_t));
}

UNIV_INLINE
byte*
com_shm_get_data(
/*=============*/
	com_shm_t*	area)
{
	return((byte*)com_shm_get_addr(area) + com_shm_get_addr_len(area));
}

/*************************************************************************
Initializes the shared memory communication system for this
process. */
UNIV_INLINE
void
com_shm_init(void)
/*==============*/
{
	mutex_create(&com_shm_destination_mutex);

	mutex_set_level(&com_shm_destination_mutex, SYNC_ANY_LATCH);

	com_shm_destination_cache = hash_create(1000);

	UT_LIST_INIT(com_shm_destination_list);

	com_shm_initialized = TRUE;
}

/*************************************************************************
Reserves the ip mutex of the shared memory area of an endpoint. */
UNIV_INLINE
void
com_shm_enter(
/*==========*/
	com_shm_endpoint_t*	ep)
{
	ulint	ret;

	ret = ip_mutex_enter(ep->ip_mutex, 10000000);

	if (ret != 0) {
		mutex_list_print_info();

		ut_error;
	}
}

/*************************************************************************
Releases the ip mutex of the shared memory area of an endpoint. */
UNIV_INLINE
void
com_shm_exit(
/*=========*/
	com_shm_endpoint_t*	ep)
{
	ip_mutex_exit(ep->ip_mutex);
}

/*************************************************************************
Looks for the given address in the cached destination addresses. */
UNIV_INLINE
com_shm_endpoint_t*
com_shm_destination_cache_search(
/*=============================*/
			/* out: cached endpoint structure if found, else NULL */
	char*	addr,	/* in: destination address */
	ulint	len)	/* in: address length */
{
	com_shm_endpoint_t*	ep;
	ulint			fold;

	fold = ut_fold_binary((byte*)addr, len);
/*	
	printf("Searching dest. cache %s %lu fold %lu\n", addr, len, fold);
*/
	mutex_enter(&com_shm_destination_mutex);

	HASH_SEARCH(addr_hash, com_shm_destination_cache, fold, ep,
			((ep->addr_len == len)
		 		&& (0 == ut_memcmp(addr, ep->addr, len))));

	mutex_exit(&com_shm_destination_mutex);

	return(ep);
}

/*************************************************************************
Inserts the given endpoint structure in the cached destination addresses. */
static
void
com_shm_destination_cache_insert(
/*=============================*/
	com_shm_endpoint_t*	ep)	/* in: endpoint struct to insert */
{
	ulint	fold;

	fold = ut_fold_binary((byte*)(ep->addr), ep->addr_len);
	
	mutex_enter(&com_shm_destination_mutex);

	/* Add to hash table */
	HASH_INSERT(com_shm_endpoint_t,
			addr_hash, com_shm_destination_cache, fold, ep);

	UT_LIST_ADD_LAST(list, com_shm_destination_list, ep);

/*	printf("Inserting to dest. cache %s %lu fold %lu\n", ep->addr,
					ep->addr_len, fold);
*/
	mutex_exit(&com_shm_destination_mutex);
}

/*************************************************************************
Frees the endpoint structs in the destination cache if the bind count is zero.
If it is not, some send operation may just be using a cached endpoint and it
cannot be freed. */
static
void
com_shm_destination_cache_no_binds(void)
/*====================================*/
{
	com_shm_endpoint_t*	ep;
	ulint			fold;

	mutex_enter(&com_shm_destination_mutex);

	if (com_shm_bind_count != 0) {
		mutex_exit(&com_shm_destination_mutex);

		return;
	}

	while (UT_LIST_GET_LEN(com_shm_destination_list) != 0) {

		ep = UT_LIST_GET_FIRST(com_shm_destination_list);

		UT_LIST_REMOVE(list, com_shm_destination_list, ep);

		fold = ut_fold_binary((byte*)ep->addr, ep->addr_len);
/*	
		printf("Deleting from dest. cache %s %lu fold %lu\n",
					ep->addr,
					ep->addr_len, fold);
*/
		HASH_DELETE(com_shm_endpoint_t, addr_hash,
					com_shm_destination_cache, fold, ep);

		com_shm_endpoint_free(ep);
	}	

	mutex_exit(&com_shm_destination_mutex);
}

/***********************************************************************
Unbinds an endpoint at the time of freeing. */
static
void
com_shm_unbind(
/*===========*/
	com_shm_endpoint_t*	ep)	/* in: endpoint */
{
	com_shm_t*	map;

	map = com_shm_endpoint_get_map(ep);

	/* Mark the shared memory area invalid */

	com_shm_set_valid(map, 0);

	/* Decrement the count of bindings */

	mutex_enter(&com_shm_destination_mutex);

	com_shm_bind_count--;

	mutex_exit(&com_shm_destination_mutex);

	/* If there are no binds left, free the cached endpoints */

	com_shm_destination_cache_no_binds();
}

/*************************************************************************
Creates a communications endpoint. */

com_shm_endpoint_t*
com_shm_endpoint_create(void)
/*=========================*/
			/* out, own: communications endpoint, NULL if
			did not succeed */
{
	com_shm_endpoint_t*	ep;

	if (!com_shm_initialized) {

		com_shm_init();
	}
	
	ep = mem_alloc(sizeof(com_shm_endpoint_t));
	
	com_shm_endpoint_set_owns_shm(ep, FALSE);
	com_shm_endpoint_set_addr(ep, NULL);
	com_shm_endpoint_set_size(ep, 0);
	
	return(ep);	
}

/*************************************************************************
Frees a communications endpoint. */

ulint
com_shm_endpoint_free(
/*==================*/
				/* out: O if succeed, else error number */
	com_shm_endpoint_t* ep)	/* in, own: communications endpoint */
{
	com_shm_t*	map;	
	
	ut_ad(ep);

	if (com_shm_endpoint_get_addr(ep) != NULL) {
		
		map = com_shm_endpoint_get_map(ep);
		
		if (com_shm_endpoint_get_owns_shm(ep)) {

			com_shm_unbind(ep);
		}
		
		/* We do not destroy the data structures in the shared memory
		area, because we cannot be sure that there is currently no
		process accessing it. Therefore we just close the ip_mutex
		residing in the area. */

		ip_mutex_close(ep->ip_mutex);

		os_event_free(com_shm_endpoint_get_not_empty(ep));
		os_event_free(com_shm_endpoint_get_empty(ep));
		
		os_shm_unmap(map);
		os_shm_free(com_shm_endpoint_get_shm(ep));

		mem_free(com_shm_endpoint_get_addr(ep));
	}

	mem_free(ep);

	return(0);
}

/*************************************************************************
Sets an option, like the maximum datagram size for an endpoint.
The options may vary depending on the endpoint type. */

ulint
com_shm_endpoint_set_option(
/*========================*/
				/* out: 0 if succeed, else error number */
	com_shm_endpoint_t* ep,	/* in: endpoint */
	ulint		optno,	/* in: option number, only
				COM_OPT_MAX_DGRAM_SIZE currently supported */
	byte*		optval,	/* in: pointer to a buffer containing the
				option value to set */
	ulint		optlen)	/* in: option value buffer length */
{
	ulint	size;

	UT_NOT_USED(optlen);

	ut_ad(ep);
	ut_a(optno == COM_OPT_MAX_DGRAM_SIZE);
	ut_ad(NULL == com_shm_endpoint_get_addr(ep));
 
	size = *((ulint*)optval);

	ut_ad(size > 0);

	com_shm_endpoint_set_size(ep, size);

	return(0);
}
	
/*************************************************************************
This function is used either to create a new shared memory area or open an
existing one, but this does not do the operations necessary with the ip mutex.
They are performed in com_shm_bind or com_shm_open which call this function. */
static
ulint
com_shm_create_or_open(
/*===================*/
				/* out: 0 if succeed, else error number */
	com_shm_endpoint_t* ep,	/* in: communications endpoint */
	char*		name,	/* in: address name */
	ulint		len)	/* in: address name length */
{
	os_shm_t	shm;
	com_shm_t*	map;
	os_event_t	event_ne;
	os_event_t	event_em;
	char*		buf;
	
	ut_ad(ep);
	ut_ad(name);
	ut_ad(len > 0);

	buf = mem_alloc(COM_MAX_ADDR_LEN + 20);

	ut_memcpy(buf, name, len);

	ut_strcpy(buf + len, "_IBSHM");

	shm = os_shm_create(sizeof(com_shm_info_t) + COM_MAX_ADDR_LEN +
			    		com_shm_endpoint_get_size(ep), buf);
	if (shm == NULL) {

		return(COM_ERR_NOT_SPECIFIED);
	}

	map = os_shm_map(shm);

	if (map == NULL) {
		os_shm_free(shm);

		return(COM_ERR_NOT_SPECIFIED);
	}

	ut_strcpy(buf + len, "_IBSHM_EV_NE"),

	event_ne = os_event_create_auto(buf);

	ut_ad(event_ne);

	ut_strcpy(buf + len, "_IBSHM_EV_EM"),

	event_em = os_event_create_auto(buf);

	ut_ad(event_em);

	com_shm_endpoint_set_shm(ep, shm);
	com_shm_endpoint_set_map(ep, map);

	com_shm_endpoint_set_not_empty(ep, event_ne);
	com_shm_endpoint_set_empty(ep, event_em);

	com_shm_endpoint_set_addr(ep, buf);
	com_shm_endpoint_set_addr_len(ep, len);

	return(0);
}

/*************************************************************************
Opens a shared memory area for communication. */
static
ulint
com_shm_open(
/*=========*/
				/* out: 0 if succeed, else error number */
	com_shm_endpoint_t* ep,	/* in: communications endpoint */
	char*		name,	/* in: address name */
	ulint		len)	/* in: address name length */
{
	ip_mutex_hdl_t*	ip_hdl;
	com_shm_t*	map;
	ulint		ret;
	char		buf[COM_MAX_ADDR_LEN + 20];
	
	ret = com_shm_create_or_open(ep, name, len);	

	if (ret != 0) {

		return(ret);
	}

	map = com_shm_endpoint_get_map(ep);

	/* Open the interprocess mutex to protect the shared memory area */

	ut_memcpy(buf, name, len);
	ut_strcpy(buf + len, "_IBSHM_MTX");
	
	ret = ip_mutex_open(com_shm_get_ip_mutex(map), buf, &ip_hdl);
	
	if (ret != 0) {

		return(COM_ERR_NOT_SPECIFIED);
	}
	
	ep->ip_mutex = ip_hdl;

	return(0);
}

/*************************************************************************
Creates a shared memory area for communication. */

ulint
com_shm_bind(
/*=========*/
				/* out: 0 if succeed, else error number */
	com_shm_endpoint_t* ep,	/* in: communications endpoint */
	char*		name,	/* in: address name */
	ulint		len)	/* in: address name length */
{
	com_shm_t*	map;
	ulint		ret;
	char		buf[COM_MAX_ADDR_LEN + 20];
	ip_mutex_hdl_t*	ip_hdl;
	
	if (com_shm_endpoint_get_size(ep) == 0) {

		return(COM_ERR_MAX_DATAGRAM_SIZE_NOT_SET);
	}
	
	ret = com_shm_create_or_open(ep, name, len);	

	if (ret != 0) {

		return(ret);
	}

	map = com_shm_endpoint_get_map(ep);

	/* Create the interprocess mutex to protect the shared memory area */

	ut_memcpy(buf, name, len);
	ut_strcpy(buf + len, "_IBSHM_MTX");
	
	ret = ip_mutex_create(com_shm_get_ip_mutex(map), buf, &ip_hdl);
	
	if (ret != 0) {

		return(COM_ERR_NOT_SPECIFIED);
	}
	
	/* This endpoint structure owns the shared memory area */

	com_shm_endpoint_set_owns_shm(ep, TRUE);
	ep->ip_mutex = ip_hdl;

	mutex_enter(&com_shm_destination_mutex);

	/* Increment the count of successful bindings */

	com_shm_bind_count++;

	mutex_exit(&com_shm_destination_mutex);

	com_shm_set_not_empty(map, FALSE);
	com_shm_set_empty_waiters(map, 0);
	com_shm_set_max_len(map, com_shm_endpoint_get_size(ep));

	com_shm_set_valid(map, COM_SHM_VALID);

	os_event_set(com_shm_endpoint_get_empty(ep));

	return(0);
}	

/*************************************************************************
Waits for a datagram to arrive at an endpoint. */

ulint
com_shm_recvfrom(
/*=============*/
				/* out: 0 if succeed, else error number */
	com_shm_endpoint_t* ep,	/* in: communications endpoint */
	byte*		buf,	/* out: datagram buffer; the buffer is
				supplied by the caller */
	ulint		buf_len,/* in: datagram buffer length */
	ulint*		len,	/* out: datagram length */
	char*		from,	/* out: address name buffer; the buffer is
				supplied by the caller */
	ulint		from_len,/* in: address name buffer length */
	ulint*		addr_len)/* out: address name length */
{
	com_shm_t*	map;
	ulint		loop_count;

	map = com_shm_endpoint_get_map(ep);

	loop_count = 0;
loop:
	com_shm_system_call_count++;

	/* NOTE: automatic event */
	
	os_event_wait(com_shm_endpoint_get_not_empty(ep));

	loop_count++;

	if (loop_count > 1) {
		printf("!!!!!!!!COM_SHM loop count %lu\n", loop_count);
	}

	ut_ad(loop_count < 2);

#ifdef notdefined
	if (!com_shm_get_not_empty(map)) {

		/* There was no datagram, give up the time slice
		for some writer thread to insert a datagram */

		com_shm_exit(ep);

		os_thread_yield();

		com_shm_enter(ep);
	}
#endif
	com_shm_enter(ep);

	if (!com_shm_get_not_empty(map)) {
		/* There was no datagram, wait for the event */

		com_shm_exit(ep);

		goto loop;
	}
		
	if (com_shm_get_data_len(map) > buf_len) {

		com_shm_exit(ep);

		return(COM_ERR_DATA_BUFFER_TOO_SMALL);
	}

	if (com_shm_get_addr_len(map) > from_len) {

		com_shm_exit(ep);

		return(COM_ERR_ADDR_BUFFER_TOO_SMALL);
	}

	*len = com_shm_get_data_len(map);
	*addr_len = com_shm_get_addr_len(map);

	ut_memcpy(buf, com_shm_get_data(map), *len);
	ut_memcpy(from, com_shm_get_addr(map), *addr_len);

	com_shm_set_not_empty(map, FALSE);

	/* If there may be writers queuing to insert the datagram, signal the
	empty-event */

	if (com_shm_get_empty_waiters(map) != 0) {

		com_shm_system_call_count++;

		os_event_set(com_shm_endpoint_get_empty(ep));
	}
	
	com_shm_exit(ep);

	return(0);
}

/*************************************************************************
Sends a datagram to the specified destination. */

ulint
com_shm_sendto(
/*===========*/
				/* out: 0 if succeed, else error number */
	com_shm_endpoint_t* ep,	/* in: communications endpoint */
	byte*		buf,	/* in: datagram buffer */
	ulint		len,	/* in: datagram length */
	char*		to,	/* in: address name buffer */
	ulint		tolen)	/* in: address name length */
{
	com_shm_endpoint_t*	ep2;
	com_shm_t*		map;
	ulint			sender_len;
	ulint			ret;
	ulint			loop_count;
	
	/* Try first to find from the cached destination addresses */

	ep2 = com_shm_destination_cache_search(to, tolen);

	if (ep2 == NULL) {
		/* Did not find it in the cache */
		ep2 = com_shm_endpoint_create();

		ret = com_shm_open(ep2, to, tolen);

		if (ret != 0) {
			com_shm_endpoint_free(ep2);

			return(ret);
		}

		/* Insert into the cached destination addresses */

		com_shm_destination_cache_insert(ep2);
	}

	map = com_shm_endpoint_get_map(ep2);

	if (com_shm_get_valid(map) != COM_SHM_VALID) {

		com_shm_exit(ep2);

		return(COM_ERR_DGRAM_NOT_DELIVERED);
	}

	if (com_shm_get_max_len(map) < len) {

		com_shm_exit(ep2);

		return(COM_ERR_DATA_TOO_LONG);
	}

	/* Optimistically, we first go to see if the datagram area is empty,
	without waiting for the empty-event */

	loop_count = 0;
loop:
	loop_count++;

	if (loop_count > 5) {
		printf("!!!!!!COM_SHM Notempty loop count %lu\n", loop_count);
	}

	ut_ad(loop_count < 100);

	com_shm_enter(ep2);

	if (com_shm_get_not_empty(map)) {

		/* Not empty, we cannot insert a datagram */

		com_shm_set_empty_waiters(map,
					1 + com_shm_get_empty_waiters(map));
		com_shm_exit(ep2);

		com_shm_system_call_count++;

		/* Wait for the area to become empty */
		/* NOTE: automatic event */

		ret = os_event_wait_time(com_shm_endpoint_get_empty(ep2),
								10000000);
		ut_a(ret == 0);

		com_shm_enter(ep2);

		com_shm_set_empty_waiters(map,
					com_shm_get_empty_waiters(map) - 1);
		com_shm_exit(ep2);

		goto loop;
	}
		
	sender_len = com_shm_endpoint_get_addr_len(ep);
	
	com_shm_set_data_len(map, len);
	com_shm_set_addr_len(map, sender_len);

	ut_memcpy(com_shm_get_data(map), buf, len);
	ut_memcpy(com_shm_get_addr(map), com_shm_endpoint_get_addr(ep),
					 			sender_len);
	com_shm_set_not_empty(map, TRUE);
#ifdef notdefined
	com_shm_exit(ep2);

	/* Now we give up our time slice voluntarily to give some reader
	thread chance to fetch the datagram */

	os_thread_yield();

	com_shm_enter(ep2);

	if (com_shm_get_not_empty(map)) {
#endif
		com_shm_system_call_count++;

		com_shm_exit(ep2);

		/* Signal the event */

		os_event_set(com_shm_endpoint_get_not_empty(ep2));

		return(0);

#ifdef notdefined
	}

	com_shm_exit(ep2);
	
	return(0);
#endif
}
