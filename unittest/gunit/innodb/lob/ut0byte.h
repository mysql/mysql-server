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
#ifndef _ut0byte_h_
#define _ut0byte_h_

#include <stdint.h>

/** The following function rounds up a pointer to the nearest aligned address.
@return aligned pointer */
inline
void*
ut_align(
        const void*     ptr,            /*!< in: pointer */
        ulint           align_no)       /*!< in: align by this number */
{
        ut_ad(align_no > 0);
        ut_ad(((align_no - 1) & align_no) == 0);
        ut_ad(ptr != nullptr);

        ut_ad(sizeof(void*) == sizeof(ulint));

        return((void*)((((intptr_t) ptr) + align_no - 1) & ~(align_no - 1)));
}

inline
void*
ut_align_down(
        const void*     ptr,
        ulint           align_no)
{
        ut_ad(align_no > 0);
        ut_ad(((align_no - 1) & align_no) == 0);
        ut_ad(ptr != nullptr);

        ut_ad(sizeof(void*) == sizeof(ulint));

        return((void*)((((intptr_t) ptr)) & ~(align_no - 1)));
}

inline
ulint
ut_align_offset(
        const void*     ptr,
        ulint           align_no)
{
        ut_ad(align_no > 0);
        ut_ad(((align_no - 1) & align_no) == 0);
        ut_ad(ptr != nullptr);

        ut_ad(sizeof(void*) == sizeof(ulint));

        return(((intptr_t) ptr) & (align_no - 1));
}

/*******************************************************//**
Creates a 64-bit integer out of two 32-bit integers.
@return created integer */
inline
ib_uint64_t
ut_ull_create(
        ulint   high,   /*!< in: high-order 32 bits */
        ulint   low)    /*!< in: low-order 32 bits */
{
        ut_ad(high <= ULINT32_MASK);
        ut_ad(low <= ULINT32_MASK);
        return(((ib_uint64_t) high) << 32 | low);
}

#endif // _ut0byte_h_
