/* Copyright (c) 2016, 2017, Oracle and/or its affiliates. All rights reserved.

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

#ifndef XCOM_CFG_H
#define XCOM_CFG_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct cfg_app_xcom
{
  /*
   The number of spin loops the XCom thread does before
   blocking on the poll system call.
  */
  unsigned int m_poll_spin_loops;

  /*
   cache size limit and interval
  */
  size_t cache_limit;
} cfg_app_xcom_st;

/*
 The application will set this pointer before engaging
 xcom
*/
extern cfg_app_xcom_st* the_app_xcom_cfg;

void init_cfg_app_xcom();
void deinit_cfg_app_xcom();

#ifdef __cplusplus
}
#endif

#endif




