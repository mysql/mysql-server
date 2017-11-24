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
#ifndef _fut0fut_h_
#define _fut0fut_h_

#include "lot0buf.h"
#include "fil0fil.h"

inline byte *fut_get_ptr(fil_addr_t addr, buf_block_t **ptr_block = nullptr) {
  buf_block_t *block;
  byte *ptr;

  ut_ad(addr.boffset < UNIV_PAGE_SIZE);

  block = buf_page_get(addr.page);
  ptr = buf_block_get_frame(block) + addr.boffset;

  if (ptr_block != NULL) {
    *ptr_block = block;
  }

  return (ptr);
}

#endif // _fut0fut_h_
