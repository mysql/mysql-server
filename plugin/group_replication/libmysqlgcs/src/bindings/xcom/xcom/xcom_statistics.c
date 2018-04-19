/* Copyright (c) 2015, 2018, Oracle and/or its affiliates. All rights reserved.

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

#include <assert.h>
#include <stdlib.h>

#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/xcom_profile.h"
#ifndef XCOM_STANDALONE
#include "my_compiler.h"
#endif
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/simset.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/task.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/task_debug.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/x_platform.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/xcom_statistics.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/xcom_vp_str.h"
#include "plugin/group_replication/libmysqlgcs/xdr_gen/xcom_vp.h"

#define M_F_SZ 19
#define M_F_MIDDLE ((M_F_SZ + 1) / 2)
#define M_F_MAX (M_F_SZ - 1)

#define STAT_INTERVAL 10.0

uint64_t send_count[LAST_OP];
uint64_t receive_count[LAST_OP];
uint64_t send_bytes[LAST_OP];
uint64_t receive_bytes[LAST_OP];

static double median_filter[M_F_SZ];
static int filter_index = 0;

#define SWAP_DBL(x, y) \
  {                    \
    double tmp = (x);  \
    (x) = (y);         \
    (y) = tmp;         \
  }

/* Partition about pivot_index */
static int qpartition(double *list, int left, int right, int pivot_index) {
  double pivot_value = list[pivot_index];
  int store_index = 0;
  int i = 0;

  SWAP_DBL(list[pivot_index], list[right]); /* Move pivot to end */
  store_index = left;
  for (i = left; i < right; i++) {
    if (list[i] <= pivot_value) {
      SWAP_DBL(list[store_index], list[i]);
      store_index++;
    }
  }
  SWAP_DBL(list[right], list[store_index]); /* Move pivot to its final place */
  return store_index;
}

/* Find k-th smallest element */
static double qselect(double *list, int left, int right, int k) {
  for (;;) {
    /* select pivot_index between left and right */
    int new_pivot = qpartition(list, left, right, right);
    {
      int pivot_dist = new_pivot - left + 1;
      if (pivot_dist == k) {
        return list[new_pivot];
      } else if (k < pivot_dist) {
        right = new_pivot - 1;
      } else {
        k = k - pivot_dist;
        left = new_pivot + 1;
      }
    }
  }
}

static int added = 1;
static double cached = 0.0;

void add_to_filter(double t) {
  median_filter[filter_index++] = t;
  if (filter_index > M_F_MAX) filter_index = 0;
  added = 1;
}

void median_filter_init() {
  int i = 0;
  for (i = 0; i < M_F_SZ; i++) add_to_filter(1.0);
}

double median_time() {
  if (!added) {
    return cached;
  } else {
    static double tmp[M_F_SZ];
    added = 0;
    memcpy(tmp, median_filter, sizeof(tmp));
    return cached = qselect(tmp, 0, M_F_MAX, M_F_MIDDLE);
  }
}
/* purecov: begin deadcode */
int xcom_statistics(task_arg arg MY_ATTRIBUTE((unused))) {
  pax_op i = 0;
  DECL_ENV
  double next;
  END_ENV;

  TASK_BEGIN
  for (i = 0; i < LAST_OP; i++) {
    send_count[i] = 0;
    receive_count[i] = 0;
    send_bytes[i] = 0;
    receive_bytes[i] = 0;
  }
  ep->next = seconds() + STAT_INTERVAL;
  TASK_DELAY_UNTIL(ep->next);
  for (;;) {
    G_DEBUG("%27s%12s%12s%12s%12s", " ", "send cnt", "receive cnt", "send b",
            "receive b");
    for (i = 0; i < LAST_OP; i++) {
      if (send_count[i] || receive_count[i]) {
        G_DEBUG("%27s%12lu%12lu%12lu%12lu", pax_op_to_str(i),
                (unsigned long)send_count[i], (unsigned long)receive_count[i],
                (unsigned long)send_bytes[i], (unsigned long)receive_bytes[i]);
      }
    }
    for (i = 0; i < LAST_OP; i++) {
      send_count[i] = 0;
      receive_count[i] = 0;
      send_bytes[i] = 0;
      receive_bytes[i] = 0;
    }
    ep->next += STAT_INTERVAL;
    TASK_DELAY_UNTIL(ep->next);
  }
  FINALLY
  TASK_END;
}
/* purecov: end */
