#ifndef MY_XXHASH_H_INCLUDED
#define MY_XXHASH_H_INCLUDED

/*
  Copyright (c) 2016, Oracle and/or its affiliates. All rights reserved.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; version 2 of the License.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software Foundation,
  51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA
*/

// Define a namespace prefix to all xxhash functions. This is done to
// avoid conflict with xxhash symbols in liblz4.
#define XXH_NAMESPACE MY_

#include "xxhash.h"

#endif // MY_XXHASH_H_INCLUDED
