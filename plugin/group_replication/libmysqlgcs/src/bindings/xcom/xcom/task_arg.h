/* Copyright (c) 2016, 2018, Oracle and/or its affiliates. All rights reserved.

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
