/*****************************************************************************

Copyright (c) 1995, 2023, Oracle and/or its affiliates.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License, version 2.0, as published by the
Free Software Foundation.

This program is also distributed with certain software (including but not
limited to OpenSSL) that is licensed under separate terms, as designated in a
particular file or component or in included license documentation. The authors
of MySQL hereby grant you an additional permission to link the program and
your derivative works with the separately licensed software that they have
included with MySQL.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License, version 2.0,
for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

*****************************************************************************/

/*
 * Start of xa.h header
 *
 * Define a symbol to prevent multiple inclusions of this header file
 */
#ifndef XA_H
#define XA_H

#include "sql/xa.h"

/*
 * Transaction branch identification: XID and NULLXID:
 */
#ifndef XIDDATASIZE

/** Sizes of transaction identifier */
#define XIDDATASIZE                                        \
  128                   /*!< maximum size of a transaction \
                        identifier, in bytes */
#define MAXGTRIDSIZE 64 /*!< maximum size in bytes of gtrid */
#define MAXBQUALSIZE 64 /*!< maximum size in bytes of bqual */

#endif

#endif /* ifndef XA_H */
/*
 * End of xa.h header
 */
