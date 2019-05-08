/* Copyright (c) 2018, Oracle and/or its affiliates. All rights reserved.

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
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/checked_data.h"
#include <stdlib.h>  // malloc
#include <string.h>  // memcpy

bool copy_checked_data(checked_data *const to, checked_data const *const from) {
  bool copied = false;

  to->data_len = 0;
  to->data_val = malloc(from->data_len * sizeof(char));
  if (to->data_val != NULL) {
    to->data_len = from->data_len;
    memcpy(to->data_val, from->data_val, from->data_len);
    copied = true;
  }

  return copied;
}
