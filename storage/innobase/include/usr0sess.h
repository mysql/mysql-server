/*****************************************************************************

Copyright (c) 1996, 2022, Oracle and/or its affiliates.

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

/** @file include/usr0sess.h
 Sessions

 Created 6/25/1996 Heikki Tuuri
 *******************************************************/

#ifndef usr0sess_h
#define usr0sess_h

#include "data0data.h"
#include "que0types.h"
#include "rem0rec.h"
#include "srv0srv.h"
#include "trx0types.h"
#include "univ.i"
#include "usr0types.h"
#include "ut0byte.h"

/** Opens a session.
 @return own: session object */
sess_t *sess_open(void);
/** Closes a session, freeing the memory occupied by it. */
void sess_close(sess_t *sess); /* in, own: session object */

/* The session handle. This data structure is only used by purge and is
not really necessary. We should get rid of it. */
struct sess_t {
  ulint state; /*!< state of the session */
  trx_t *trx;  /*!< transaction object permanently
               assigned for the session: the
               transaction instance designated by the
               trx id changes, but the memory
               structure is preserved */
};

/* Session states */
constexpr uint32_t SESS_ACTIVE = 1;
/** session contains an error message which has not yet been communicated to the
client */
constexpr uint32_t SESS_ERROR = 2;

#endif
