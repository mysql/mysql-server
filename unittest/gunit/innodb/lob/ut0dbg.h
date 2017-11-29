/*****************************************************************************

Copyright (c) 2016, 2017 Oracle and/or its affiliates. All Rights Reserved.

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
#ifndef _ut0dbg_h_
#define _ut0dbg_h_

#include "lot0types.h"

void
ut_dbg_assertion_failed(
        const char*     expr,
        const char*     file,
        ulint           line);

#define ut_a(EXPR) do {                                         \
        if (!(ulint) (EXPR)) {                   \
                ut_dbg_assertion_failed(#EXPR,                  \
                                __FILE__, (ulint) __LINE__);    \
        }                                                       \
} while (0)

/** Abort execution. */
#define ut_error                                                \
        ut_dbg_assertion_failed(0, __FILE__, (ulint) __LINE__)

/** Debug assertion. Does nothing unless UNIV_DEBUG is defined. */
#define ut_ad(EXPR)     ut_a(EXPR)
/** Debug statement. Does nothing unless UNIV_DEBUG is defined. */
#define ut_d(EXPR)      EXPR

#endif // _ut0dbg_h_
