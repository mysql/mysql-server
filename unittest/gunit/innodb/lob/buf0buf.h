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

#ifndef _buf0buf_h_
#define _buf0buf_h_

#include "lot0types.h"
#include "fil0types.h"
#include "fil0fil.h"

inline
void
buf_ptr_get_fsp_addr(
        const void*     ptr,
        space_id_t*     space,
        fil_addr_t*     addr)
{
        const page_t*   page = (const page_t*) ut_align_down(ptr,
                                                             UNIV_PAGE_SIZE);

        *space = mach_read_from_4(page + FIL_PAGE_ARCH_LOG_NO_OR_SPACE_ID);
        addr->page = mach_read_from_4(page + FIL_PAGE_OFFSET);
        addr->boffset = ut_align_offset(ptr, UNIV_PAGE_SIZE);
}

#endif // _buf0buf_h_
