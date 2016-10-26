/* Copyright (c) 2016, Oracle and/or its affiliates. All rights reserved.

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

#ifndef TASK_ARG_H
#define TASK_ARG_H

#ifdef __cplusplus
extern "C" {
#endif

/* Reasonably type-safe parameters to tasks */
enum arg_type {
	a_int,
	a_long,
	a_uint,
	a_ulong,
	a_ulong_long,
	a_float,
	a_double,
	a_void,
	a_string,
	a_end
};
typedef enum arg_type arg_type;

struct task_arg {
	arg_type type;
	union {
		int i;
		long l;
		unsigned int u_i;
		unsigned long u_l;
		unsigned long long u_ll;
		float f;
		double d;
		char const *s;
		void *v;
	} val;
};
typedef struct task_arg task_arg;

#ifdef __cplusplus
}
#endif

#endif
