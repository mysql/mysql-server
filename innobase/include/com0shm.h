/******************************************************
The communication through shared memory

(c) 1995 Innobase Oy

Created 9/23/1995 Heikki Tuuri
*******************************************************/

#ifndef com0shm_h
#define com0shm_h

#include "univ.i"

typedef struct com_shm_endpoint_struct	com_shm_endpoint_t;

/* The performance of communication in NT depends on how
many times a system call is made (excluding os_thread_yield,
as that is the fastest way to switch thread).
The following variable counts such events. */

extern ulint	com_shm_system_call_count;


/*************************************************************************
Creates a communications endpoint. */

com_shm_endpoint_t*
com_shm_endpoint_create(void);
/*=========================*/
			/* out, own: communications endpoint, NULL if
			did not succeed */
/*************************************************************************
Frees a communications endpoint. */

ulint
com_shm_endpoint_free(
/*==================*/
				/* out: O if succeed, else error number */
	com_shm_endpoint_t* ep);/* in, own: communications endpoint */
/*************************************************************************
Sets an option, like the maximum datagram size for an endpoint.
The options may vary depending on the endpoint type. */

ulint
com_shm_endpoint_set_option(
/*========================*/
				/* out: 0 if succeed, else error number */
	com_shm_endpoint_t* 	ep,	/* in: endpoint */
	ulint		optno,	/* in: option number, only
				COM_OPT_MAX_DGRAM_SIZE currently supported */
	byte*		optval,	/* in: pointer to a buffer containing the
				option value to set */
	ulint		optlen);/* in: option value buffer length */
/*************************************************************************
Bind a communications endpoint to a specified address. */

ulint
com_shm_bind(
/*=========*/
				/* out: 0 if succeed, else error number */
	com_shm_endpoint_t* ep,	/* in: communications endpoint */
	char*		name,	/* in: address name */
	ulint		len);	/* in: address name length */
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
	ulint*		addr_len);/* out: address name length */
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
	ulint		tolen);	/* in: address name length */

ulint
com_shm_endpoint_get_size(
/*======================*/
	com_shm_endpoint_t*	ep);


#ifndef UNIV_NONINL
#include "com0shm.ic"
#endif

#endif
