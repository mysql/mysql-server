/* Copyright (c) 2015, 2023, Oracle and/or its affiliates.

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

#ifdef _MSC_VER
#include <stdint.h>
#endif
#include "xcom/synode_no.h"

#include <assert.h>
#include <rpc/rpc.h>
#include <stdlib.h>

#include "xcom/simset.h"
#include "xcom/task.h"
#include "xcom/task_debug.h"
#include "xcom/xcom_common.h"
#include "xcom/xcom_profile.h"
#include "xcom/xdr_utils.h"
#include "xdr_gen/xcom_vp.h"

synode_no const null_synode = NULL_SYNODE;

int synode_eq(synode_no x, synode_no y) {
  return x.group_id == y.group_id && x.msgno == y.msgno && x.node == y.node;
}

int synode_lt(synode_no x, synode_no y) {
  assert(x.group_id == 0 || y.group_id == 0 || x.group_id == y.group_id);
  return (x.msgno < y.msgno) || (x.msgno == y.msgno && x.node < y.node);
}

int synode_gt(synode_no x, synode_no y) {
  assert(x.group_id == 0 || y.group_id == 0 || x.group_id == y.group_id);
  return (x.msgno > y.msgno) || (x.msgno == y.msgno && x.node > y.node);
}

/* purecov: begin deadcode */
#ifdef TASK_EVENT_TRACE
void add_synode_event(synode_no const synode) {
  add_event(0, string_arg("{"));
  add_event(EVENT_DUMP_HEX | EVENT_DUMP_PAD, uint_arg(synode.group_id));
  add_event(EVENT_DUMP_PAD, ulong_long_arg(synode.msgno));
  add_event(0, ulong_arg(synode.node));
  add_event(EVENT_DUMP_PAD, string_arg("}"));
}
#endif

void synode_array_move(synode_no_array *const to, synode_no_array *const from) {
  if (to->synode_no_array_val != nullptr) free(to->synode_no_array_val);

  *to = *from;

  from->synode_no_array_len = 0;
  from->synode_no_array_val = nullptr;
}

void synode_app_data_array_move(synode_app_data_array *const to,
                                synode_app_data_array *const from) {
  if (to->synode_app_data_array_val != nullptr) {
    free(to->synode_app_data_array_val);
  }

  *to = *from;

  from->synode_app_data_array_len = 0;
  from->synode_app_data_array_val = nullptr;
}

/* purecov: end */
