/*
   Copyright (c) 2004, 2017, Oracle and/or its affiliates. All rights reserved.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef DB_CONFIG_PARAMTERS_H
#define DB_CONFIG_PARAMTERS_H

#define PRIVATE_BASE          14000

#define CFG_ACC_FRAGMENT      (PRIVATE_BASE +  3)
#define CFG_ACC_OP_RECS       (PRIVATE_BASE +  4)
#define CFG_ACC_TABLE         (PRIVATE_BASE +  8)
#define CFG_ACC_SCAN          (PRIVATE_BASE +  9)

#define CFG_DICT_ATTRIBUTE    (PRIVATE_BASE + 10)
#define CFG_DICT_TABLE        (PRIVATE_BASE + 13)

#define CFG_DIH_FRAG_CONNECT  (PRIVATE_BASE + 17)
#define CFG_DIH_REPLICAS      (PRIVATE_BASE + 19)
#define CFG_DIH_TABLE         (PRIVATE_BASE + 20)

#define CFG_LQH_FRAG          (PRIVATE_BASE + 21)
#define CFG_LQH_TABLE         (PRIVATE_BASE + 23)
#define CFG_LQH_TC_CONNECT    (PRIVATE_BASE + 24)
#define CFG_LQH_LOG_FILES     (PRIVATE_BASE + 26)
#define CFG_LQH_SCAN          (PRIVATE_BASE + 27)

#define CFG_TC_API_CONNECT    (PRIVATE_BASE + 28)
#define CFG_TC_TC_CONNECT     (PRIVATE_BASE + 29)
#define CFG_TC_TABLE          (PRIVATE_BASE + 30)
#define CFG_TC_SCAN           (PRIVATE_BASE + 31)
#define CFG_TC_LOCAL_SCAN     (PRIVATE_BASE + 32)
  
#define CFG_TUP_FRAG          (PRIVATE_BASE + 33)
#define CFG_TUP_OP_RECS       (PRIVATE_BASE + 34)
#define CFG_TUP_PAGE          (PRIVATE_BASE + 35)
#define _CFG_TUP_PAGE_RANGE   (PRIVATE_BASE + 36)
#define CFG_TUP_TABLE         (PRIVATE_BASE + 37)
#define _CFG_TUP_TABLE_DESC   (PRIVATE_BASE + 38)
#define CFG_TUP_STORED_PROC   (PRIVATE_BASE + 39)

#define CFG_TUX_INDEX         (PRIVATE_BASE + 40)
#define CFG_TUX_FRAGMENT      (PRIVATE_BASE + 41)
#define CFG_TUX_ATTRIBUTE     (PRIVATE_BASE + 42)
#define CFG_TUX_SCAN_OP       (PRIVATE_BASE + 43)

#define CFG_SPJ_TABLE         (PRIVATE_BASE + 44)

#define CFG_TUP_NO_TRIGGERS   (PRIVATE_BASE + 45)

#endif
