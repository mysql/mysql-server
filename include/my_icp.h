/* Copyright (c) 2013, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA */

#ifndef _my_icp_h
#define _my_icp_h

#ifdef	__cplusplus
extern "C" {
#endif

/**
  Values returned by index_cond_func_xxx functions.
*/

typedef enum icp_result {
  /** Index tuple doesn't satisfy the pushed index condition (the engine
  should discard the tuple and go to the next one) */
  ICP_NO_MATCH,

  /** Index tuple satisfies the pushed index condition (the engine should
  fetch and return the record) */
  ICP_MATCH,

  /** Index tuple is out of the range that we're scanning, e.g. if we're
  scanning "t.key BETWEEN 10 AND 20" and got a "t.key=21" tuple (the engine
  should stop scanning and return HA_ERR_END_OF_FILE right away). */
  ICP_OUT_OF_RANGE

} ICP_RESULT;


#ifdef	__cplusplus
}
#endif

#endif /* _my_icp_h */
