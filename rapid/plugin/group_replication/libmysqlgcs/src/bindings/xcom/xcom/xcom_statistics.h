/* Copyright (c) 2015, 2016, Oracle and/or its affiliates. All rights reserved.

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

#ifndef XCOM_STATISTICS_H
#define XCOM_STATISTICS_H

#ifdef __cplusplus
extern "C" {
#endif


extern uint64_t	send_count[LAST_OP];
extern uint64_t	receive_count[LAST_OP];
extern uint64_t	send_bytes[LAST_OP];
extern uint64_t	receive_bytes[LAST_OP];

double	median_time();
int	xcom_statistics(task_arg arg);
void add_to_filter(double t);
void median_filter_init();



#ifdef __cplusplus
}
#endif

#endif

