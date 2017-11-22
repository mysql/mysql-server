/*****************************************************************************

Copyright (c) 1995, 2017, Oracle and/or its affiliates. All Rights Reserved.

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

#include "sql/xa.h"

/*
 * Transaction branch identification: XID and NULLXID:
 */
#ifndef XIDDATASIZE

/** Sizes of transaction identifier */
#define	XIDDATASIZE	128		/*!< maximum size of a transaction
					identifier, in bytes */
#define	MAXGTRIDSIZE	 64		/*!< maximum size in bytes of gtrid */
#define	MAXBQUALSIZE	 64		/*!< maximum size in bytes of bqual */

#endif

#endif /* ifndef XA_H */
/*
 * End of xa.h header
 */
