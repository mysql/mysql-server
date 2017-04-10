/* Copyright (c) 2005, 2016, Oracle and/or its affiliates. All rights reserved.

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; version 2 of the License.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA */

/*
  This is a header for libraries containing functions used in both server and
  only some of clients (but not in libmysql)...
*/

#ifndef _my_user_h_
#define _my_user_h_

#include <stddef.h>

#include "my_macros.h"

C_MODE_START

void parse_user(const char *user_id_str, size_t user_id_len,
                char *user_name_str, size_t *user_name_len,
                char *host_name_str, size_t *host_name_len);

C_MODE_END

#endif /* _my_user_h_ */
