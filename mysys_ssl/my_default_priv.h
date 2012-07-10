/* Copyright (c) 2012, Oracle and/or its affiliates. All rights reserved.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; version 2 of the License.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software Foundation,
  51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */

#ifndef MY_DEFAULT_PRIV_INCLUDED
#define MY_DEFAULT_PRIV_INCLUDED

#include "my_global.h"                          /* C_MODE_START, C_MODE_END */

/*
  Number of byte used to store the length of
  cipher that follows.
*/
#define MAX_CIPHER_STORE_LEN 4U
#define LOGIN_KEY_LEN 20U

C_MODE_START

/**
  Place the login file name in the specified buffer.

  @param file_name     [out]  Buffer to hold login file name
  @param file_name_size [in]  Length of the buffer

  @return 1 - Success
          0 - Failure
*/
int my_default_get_login_file(char *file_name, size_t file_name_size);

C_MODE_END

#endif /* MY_DEFAULT_PRIV_INCLUDED */
