/*****************************************************************************

Copyright (c) 1995, 2009, Oracle and/or its affiliates. All Rights Reserved.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA

*****************************************************************************/

/*
 * Start of xa.h header
 *
 * Define a symbol to prevent multiple inclusions of this header file
 */
#ifndef	XA_H
#define	XA_H

/*
 * Transaction branch identification: XID and NULLXID:
 */
#ifndef XIDDATASIZE

/** Sizes of transaction identifier */
#define	XIDDATASIZE	128		/*!< maximum size of a transaction
					identifier, in bytes */
#define	MAXGTRIDSIZE	 64		/*!< maximum size in bytes of gtrid */
#define	MAXBQUALSIZE	 64		/*!< maximum size in bytes of bqual */

/** X/Open XA distributed transaction identifier */
struct xid_t {
	long formatID;			/*!< format identifier; -1
					means that the XID is null */
	long gtrid_length;		/*!< value from 1 through 64 */
	long bqual_length;		/*!< value from 1 through 64 */
	char data[XIDDATASIZE];		/*!< distributed transaction
					identifier */
};
/** X/Open XA distributed transaction identifier */
typedef	struct xid_t XID;
#endif
/** X/Open XA distributed transaction status codes */
/* @{ */
#define	XA_OK		0		/*!< normal execution */
#define	XAER_ASYNC	-2		/*!< asynchronous operation already
					outstanding */
#define	XAER_RMERR	-3		/*!< a resource manager error
					occurred in the transaction
					branch */
#define	XAER_NOTA	-4		/*!< the XID is not valid */
#define	XAER_INVAL	-5		/*!< invalid arguments were given */
#define	XAER_PROTO	-6		/*!< routine invoked in an improper
					context */
#define	XAER_RMFAIL	-7		/*!< resource manager unavailable */
#define	XAER_DUPID	-8		/*!< the XID already exists */
#define	XAER_OUTSIDE	-9		/*!< resource manager doing
					work outside transaction */
/* @} */
#endif /* ifndef XA_H */
/*
 * End of xa.h header
 */
