/******************************************************
The communication primitives

(c) 1995 Innobase Oy

Created 9/23/1995 Heikki Tuuri
*******************************************************/

/* This module defines a standard datagram communication
function interface for use in the database. We assume that
the communication medium is reliable. */

#ifndef com0com_h
#define com0com_h

#include "univ.i"

/* The communications endpoint type definition */
typedef struct com_endpoint_struct	com_endpoint_t;

/* Possible endpoint communication types */
#define	COM_SHM		1	/* communication through shared memory */

/* Option numbers for endpoint */
#define COM_OPT_MAX_DGRAM_SIZE	1

/* Error numbers */
#define COM_ERR_NOT_SPECIFIED			1
#define COM_ERR_NOT_BOUND			2
#define COM_ERR_ALREADY_BOUND			3
#define COM_ERR_MAX_DATAGRAM_SIZE_NOT_SET	4
#define COM_ERR_DATA_BUFFER_TOO_SMALL		5
#define COM_ERR_ADDR_BUFFER_TOO_SMALL		6
#define COM_ERR_DATA_TOO_LONG			7
#define COM_ERR_ADDR_TOO_LONG			8
#define COM_ERR_DGRAM_NOT_DELIVERED		9

/* Maximum allowed address length in bytes */
#define COM_MAX_ADDR_LEN	100

/*************************************************************************
Creates a communications endpoint. */

com_endpoint_t*
com_endpoint_create(
/*================*/
			/* out, own: communications endpoint, NULL if
			did not succeed */
	ulint	type);	/* in: communication type of endpoint:
			only COM_SHM supported */
/*************************************************************************
Frees a communications endpoint. */

ulint
com_endpoint_free(
/*==============*/
				/* out: O if succeed, else error number */
	com_endpoint_t*	ep);	/* in, own: communications endpoint */
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
	ulint		optlen);/* in: option value buffer length */
/*************************************************************************
Binds a communications endpoint to a specified address. */

ulint
com_bind(
/*=====*/
				/* out: 0 if succeed, else error number */
	com_endpoint_t*	ep,	/* in: communications endpoint */
	char*		name,	/* in: address name */
	ulint		len);	/* in: name length */
/*************************************************************************
Waits for a datagram to arrive at an endpoint. */

ulint
com_recvfrom(
/*=========*/
				/* out: 0 if succeed, else error number */
	com_endpoint_t*	ep,	/* in: communications endpoint */
	byte*		buf,	/* out: datagram buffer; the buffer must be
				supplied by the caller */
	ulint		buf_len,/* in: datagram buffer length */
	ulint*		len,	/* out: datagram length */
	char*		from,	/* out: address name buffer; the buffer must be
				supplied by the caller */
	ulint		from_len,/* in: address name buffer length */
	ulint*		addr_len);/* out: address name length */
/*************************************************************************
Sends a datagram to a specified destination. */

ulint
com_sendto(
/*=======*/
				/* out: 0 if succeed, else error number */
	com_endpoint_t*	ep,	/* in: communications endpoint */
	byte*		buf,	/* in: datagram buffer */
	ulint		len,	/* in: datagram length */
	char*		to,	/* in: address name buffer */
	ulint		tolen);	/* in: address name length */
/*************************************************************************
Gets the maximum datagram size for an endpoint. */

ulint
com_endpoint_get_max_size(
/*======================*/
				/* out: maximum size */
	com_endpoint_t*	ep);	/* in: endpoint */


#ifndef UNIV_NONINL
#include "com0com.ic"
#endif

#endif
