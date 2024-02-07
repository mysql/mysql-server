/*****************************************************************************

Copyright (c) 1996, 2024, Oracle and/or its affiliates.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License, version 2.0, as published by the
Free Software Foundation.

This program is designed to work with certain software (including
but not limited to OpenSSL) that is licensed under separate terms,
as designated in a particular file or component or in included license
documentation.  The authors of MySQL hereby grant you an additional
permission to link the program and your derivative works with the
separately licensed software that they have either included with
the program or referenced in the documentation.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License, version 2.0,
for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

*****************************************************************************/

/** @file usr/usr0sess.cc
 Sessions

 Created 6/25/1996 Heikki Tuuri
 *******************************************************/

#include "usr0sess.h"
#include "trx0trx.h"

/** Opens a session.
 @return own: session object */
sess_t *sess_open(void) {
  sess_t *sess;

  sess = static_cast<sess_t *>(
      ut::zalloc_withkey(UT_NEW_THIS_FILE_PSI_KEY, sizeof(*sess)));

  sess->state = SESS_ACTIVE;

  sess->trx = trx_allocate_for_background();
  sess->trx->sess = sess;

  return (sess);
}

/** Closes a session, freeing the memory occupied by it. */
void sess_close(sess_t *sess) /*!< in, own: session object */
{
  trx_free_for_background(sess->trx);
  ut::free(sess);
}
