/* Copyright (c) 2015, 2017, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef XCOM_RECOVER_H
#define XCOM_RECOVER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/task_arg.h"

int xcom_booted();
void xcom_recover_init();
void set_log_group_id(uint32_t group_id);
int log_prefetch_task(task_arg arg);

#ifdef __cplusplus
}
#endif

#endif
