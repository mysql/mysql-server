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

#ifndef NODE_NO_H
#define NODE_NO_H

#ifdef __cplusplus
extern "C" {
#endif

#define ull_void ~((synode_cnt)0)
#define ul_void ~((u_long)0)
#define u_void ~((u_int)0)

/*
  Node numbers are usually very small. A real node_no value should never close
  to the maximun value of uint32_t. So VOID_NODE_NO is defined as the maximun
  value of uint32_t.
*/
#define VOID_NODE_NO (~((node_no)0))

#ifdef __cplusplus
}
#endif

#endif
