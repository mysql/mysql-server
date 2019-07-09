#ifndef OPT_STATISTICS_INCLUDED
#define OPT_STATISTICS_INCLUDED

/*
   Copyright (c) 2014, 2015, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */

#include "my_global.h"                          // uint

struct TABLE;
typedef float rec_per_key_t;
typedef struct st_key KEY;

/**
  Guesstimate for "records per key" when index statistics is not available.

  @param table         the table
  @param key           the index
  @param used_keyparts the number of key part that should be included in the
                       estimate

  @return estimated records per key value
*/

rec_per_key_t guess_rec_per_key(const TABLE *const table, const KEY *const key,
                                uint used_keyparts);

#endif /* OPT_STATISTICS_INCLUDED */
