/* Copyright (c) 2015, 2022, Oracle and/or its affiliates.

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

#ifndef XCOM_STATISTICS_H
#define XCOM_STATISTICS_H

#include <stdint.h>

#include "xcom/task_arg.h"
#include "xdr_gen/xcom_vp.h"

extern uint64_t send_count[LAST_OP];
extern uint64_t receive_count[LAST_OP];
extern uint64_t send_bytes[LAST_OP];
extern uint64_t receive_bytes[LAST_OP];

double median_time();
void add_to_filter(double t);
void median_filter_init();

#endif
