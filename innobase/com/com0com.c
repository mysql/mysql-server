/******************************************************
The communication primitives

(c) 1995 Innobase Oy

Created 9/23/1995 Heikki Tuuri
*******************************************************/

#include "com0com.h"
#ifdef UNIV_NONINL
#include "com0com.ic"
#endif

#include "mem0mem.h"
#include "com0shm.h"

/*
	IMPLEMENTATION OF COMMUNICATION PRIMITIVES
	==========================================

The primitives provide a uniform function interface for
use in communication. The primitives have been modeled
after the Windows Sockets interface. Below this uniform
API, the precise methods of communication, for example,
shared memory or Berkeley sockets, can be implemented
as subroutines.
*/

struct com_endpoint_struct{
	ulint	type;		/* endpoint type */
	void*	par;		/* type-specific data structures */
	ibool	bound;		/* TRUE if the endpoint has been
				bound to an address */
};

/*************************************************************************
Accessor functions for an endpoint */
UNIV_INLINE
ulint
com_endpoint_get_type(
/*==================*/
	com_endpoint_t*	ep)
{
	ut_ad(ep);
	return(ep->type);
}

UNIV_INLINE
void
com_endpoint_set_type(
/*==================*/
	com_endpoint_t*	ep,
	ulint		type)
{
	ut_ad(ep);
	ut_ad(type == COM_SHM);

	ep->type = type;
}

UNIV_INLINE
void*
com_endpoint_get_par(
/*=================*/
	com_endpoint_t*	ep)
{
	ut_ad(ep);
	return(ep->par);
}

UNIV_INLINE
void
com_endpoint_set_par(
/*=================*/
	com_endpoint_t*	ep,
	void*		par)
{
	ut_ad(ep);
	ut_ad(par);

	ep->par = par;
}

UNIV_INLINE
ibool
com_endpoint_get_bound(
/*===================*/
	com_endpoint_t*	ep)
{
	ut_ad(ep);
	return(ep->bound);
}

UNIV_INLINE
void
com_endpoint_set_bound(
/*===================*/
	com_endpoint_t*	ep,
	ibool		bound)
{
	ut_ad(ep);

	ep->bound = bound;
}


/*************************************************************************
Creates a communications endpoint. */

com_endpoint_t*
com_endpoint_create(
/*================*/
			/* out, own: communications endpoint, NULL if
			did not succeed */
	ulint	type)	/* in: communication type of endpoint:
			only COM_SHM supported */
{
	com_endpoint_t*	ep;
	void*		par;

	ep = mem_alloc(sizeof(com_endpoint_t));
	
	com_endpoint_set_type(ep, type);
	com_endpoint_set_bound(ep, FALSE);
	
	if (type == COM_SHM) {
		par = com_shm_endpoint_create();
		com_endpoint_set_par(ep, par);
	} else {
		par = NULL;
		ut_error;
	}

	if (par != NULL) {
		return(ep);
	} else {
		mem_free(ep);
		return(NULL);
	}
}

/*************************************************************************
Frees a communications endpoint. */

ulint
com_endpoint_free(
/*==============*/
				/* out: O if succeed, else error number */
	com_endpoint_t*	ep)	/* in, own: communications endpoint */
{
	ulint	type;
	ulint	ret;
	void*	par;

	type = com_endpoint_get_type(ep);
	par  = com_endpoint_get_par(ep);

	if (type == COM_SHM) {
		ret = com_shm_endpoint_free((com_shm_endpoint_t*)par);
	} else {
		ret = 0;
		ut_error;
	}

	if (ret) {
		return(ret);
	} else {
		mem_free(ep);
		return(0);
	}
}

/*************************************************************************
Sets an option, like the maximum datagram size for an endpoint.
The options may vary depending on the endpoint type. */

ulint
com_endpoint_set_option(
/*====================*/
				/* out: 0 if succeed, else error number */
	com_endpoint_t*	ep,	/* in: endpoint */
	ulint		optno,	/* in: option number, only
				COM_OPT_MAX_DGRAM_SIZE currently supported */
	byte*		optval,	/* in: pointer to a buffer containing the
				option value to set */
	ulint		optlen)	/* in: option value buffer length */
{
	ulint	type;
	ulint	ret;
	void*	par;

	type = com_endpoint_get_type(ep);
	par  = com_endpoint_get_par(ep);

	if (type == COM_SHM) {
		ret = com_shm_endpoint_set_option((com_shm_endpoint_t*)par,
						  optno, optval, optlen);
	} else {
		ret = 0;
		ut_error;
	}

	return(ret);
}
	
/*************************************************************************
Binds a communications endpoint to the specified address. */

ulint
com_bind(
/*=====*/
				/* out: 0 if succeed, else error number */
	com_endpoint_t*	ep,	/* in: communications endpoint */
	char*		name,	/* in: address name */
	ulint		len)	/* in: name length */
{
	ulint	type;
	ulint	ret;
	void*	par;

	ut_ad(len <= COM_MAX_ADDR_LEN);

	if (com_endpoint_get_bound(ep)) {
		return(COM_ERR_ALREADY_BOUND);
	}

	type = com_endpoint_get_type(ep);
	par  = com_endpoint_get_par(ep);

	if (type == COM_SHM) {
		ret = com_shm_bind((com_shm_endpoint_t*)par, name, len);
	} else {
		ret = 0;
		ut_error;
	}

	if (ret == 0) {
		com_endpoint_set_bound(ep, TRUE);
	}		
	
	return(ret);
}

/*************************************************************************
Waits for a datagram to arrive at an endpoint. */

ulint
com_recvfrom(
/*=========*/
				/* out: 0 if succeed, else error number */
	com_endpoint_t*	ep,	/* in: communications endpoint */
	byte*		buf,	/* out: datagram buffer; the buffer is
				supplied by the caller */
	ulint		buf_len,/* in: datagram buffer length */
	ulint*		len,	/* out: datagram length */
	char*		from,	/* out: address name buffer; the buffer is
				supplied by the caller */
	ulint		from_len,/* in: address name buffer length */
	ulint*		addr_len)/* out: address name length */
{
	ulint	type;
	ulint	ret;
	void*	par;

	if (!com_endpoint_get_bound(ep)) {

		return(COM_ERR_NOT_BOUND);
	}

	type = com_endpoint_get_type(ep);
	par  = com_endpoint_get_par(ep);

	if (type == COM_SHM) {
		ret = com_shm_recvfrom((com_shm_endpoint_t*)par,
					buf, buf_len, len, from, from_len,
					addr_len);
	} else {
		ret = 0;

		ut_error;
	}

	return(ret);
}

/*************************************************************************
Sends a datagram to the specified destination. */

ulint
com_sendto(
/*=======*/
				/* out: 0 if succeed, else error number */
	com_endpoint_t*	ep,	/* in: communications endpoint */
	byte*		buf,	/* in: datagram buffer */
	ulint		len,	/* in: datagram length */
	char*		to,	/* in: address name buffer */
	ulint		tolen)	/* in: address name length */
{
	ulint	type;
	ulint	ret;
	void*	par;

	if (!com_endpoint_get_bound(ep)) {
		return(COM_ERR_NOT_BOUND);
	}

	type = com_endpoint_get_type(ep);
	par  = com_endpoint_get_par(ep);

	if (type == COM_SHM) {
		ret = com_shm_sendto((com_shm_endpoint_t*)par, buf, len,
								to, tolen);
	} else {
		ret = 0;
		ut_error;
	}

	return(ret);
}

/*************************************************************************
Gets the maximum datagram size for an endpoint. */

ulint
com_endpoint_get_max_size(
/*======================*/
				/* out: maximum size */
	com_endpoint_t*	ep)	/* in: endpoint */
{
	ulint	type;
	ulint	ret;
	void*	par;

	type = com_endpoint_get_type(ep);
	par  = com_endpoint_get_par(ep);

	if (type == COM_SHM) {
		ret = com_shm_endpoint_get_size((com_shm_endpoint_t*)par);
	} else {
		ret = 0;
		ut_error;
	}

	return(ret);
}
