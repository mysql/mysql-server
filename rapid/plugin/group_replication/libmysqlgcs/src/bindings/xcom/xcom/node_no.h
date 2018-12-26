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

