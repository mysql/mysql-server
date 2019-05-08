/*
   Copyright (c) 2004, 2018, Oracle and/or its affiliates. All rights reserved.

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

#define CFG_TC_TABLE          (PRIVATE_BASE + 30)
// TODO: +28,29,31 UNUSED
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

#define CFG_TC_TARGET_FRAG_LOCATION (PRIVATE_BASE + 46)
#define CFG_TC_TARGET_SCAN_FRAGMENT (PRIVATE_BASE + 47)
#define CFG_TC_TARGET_SCAN_RECORD (PRIVATE_BASE + 48)
#define CFG_TC_TARGET_CONNECT_RECORD (PRIVATE_BASE + 49)
#define CFG_TC_TARGET_TO_CONNECT_RECORD (PRIVATE_BASE + 50)
#define CFG_TC_TARGET_COMMIT_ACK_MARKER (PRIVATE_BASE + 51)
#define CFG_TC_TARGET_TO_COMMIT_ACK_MARKER (PRIVATE_BASE + 52)
#define CFG_TC_TARGET_INDEX_OPERATION (PRIVATE_BASE + 53)
#define CFG_TC_TARGET_API_CONNECT_RECORD (PRIVATE_BASE + 54)
#define CFG_TC_TARGET_TO_API_CONNECT_RECORD (PRIVATE_BASE + 55)
#define CFG_TC_TARGET_CACHE_RECORD (PRIVATE_BASE + 56)
#define CFG_TC_TARGET_FIRED_TRIGGER_DATA (PRIVATE_BASE + 57)
#define CFG_TC_TARGET_ATTRIBUTE_BUFFER (PRIVATE_BASE + 58)
#define CFG_TC_TARGET_COMMIT_ACK_MARKER_BUFFER (PRIVATE_BASE + 59)
#define CFG_TC_TARGET_TO_COMMIT_ACK_MARKER_BUFFER (PRIVATE_BASE + 60)

#define CFG_TC_MAX_FRAG_LOCATION (PRIVATE_BASE + 61)
#define CFG_TC_MAX_SCAN_FRAGMENT (PRIVATE_BASE + 62)
#define CFG_TC_MAX_SCAN_RECORD (PRIVATE_BASE + 63)
#define CFG_TC_MAX_CONNECT_RECORD (PRIVATE_BASE + 64)
#define CFG_TC_MAX_TO_CONNECT_RECORD (PRIVATE_BASE + 65)
#define CFG_TC_MAX_COMMIT_ACK_MARKER (PRIVATE_BASE + 66)
#define CFG_TC_MAX_TO_COMMIT_ACK_MARKER (PRIVATE_BASE + 67)
#define CFG_TC_MAX_INDEX_OPERATION (PRIVATE_BASE + 68)
#define CFG_TC_MAX_API_CONNECT_RECORD (PRIVATE_BASE + 69)
#define CFG_TC_MAX_TO_API_CONNECT_RECORD (PRIVATE_BASE + 70)
#define CFG_TC_MAX_CACHE_RECORD (PRIVATE_BASE + 71)
#define CFG_TC_MAX_FIRED_TRIGGER_DATA (PRIVATE_BASE + 72)
#define CFG_TC_MAX_ATTRIBUTE_BUFFER (PRIVATE_BASE + 73)
#define CFG_TC_MAX_COMMIT_ACK_MARKER_BUFFER (PRIVATE_BASE + 74)
#define CFG_TC_MAX_TO_COMMIT_ACK_MARKER_BUFFER (PRIVATE_BASE + 75)

#define CFG_TC_RESERVED_FRAG_LOCATION (PRIVATE_BASE + 76)
#define CFG_TC_RESERVED_SCAN_FRAGMENT (PRIVATE_BASE + 77)
#define CFG_TC_RESERVED_SCAN_RECORD (PRIVATE_BASE + 78)
#define CFG_TC_RESERVED_CONNECT_RECORD (PRIVATE_BASE + 79)
#define CFG_TC_RESERVED_TO_CONNECT_RECORD (PRIVATE_BASE + 80)
#define CFG_TC_RESERVED_COMMIT_ACK_MARKER (PRIVATE_BASE + 81)
#define CFG_TC_RESERVED_TO_COMMIT_ACK_MARKER (PRIVATE_BASE + 82)
#define CFG_TC_RESERVED_INDEX_OPERATION (PRIVATE_BASE + 83)
#define CFG_TC_RESERVED_API_CONNECT_RECORD (PRIVATE_BASE + 84)
#define CFG_TC_RESERVED_TO_API_CONNECT_RECORD (PRIVATE_BASE + 85)
#define CFG_TC_RESERVED_CACHE_RECORD (PRIVATE_BASE + 86)
#define CFG_TC_RESERVED_FIRED_TRIGGER_DATA (PRIVATE_BASE + 87)
#define CFG_TC_RESERVED_ATTRIBUTE_BUFFER (PRIVATE_BASE + 88)
#define CFG_TC_RESERVED_COMMIT_ACK_MARKER_BUFFER (PRIVATE_BASE + 89)
#define CFG_TC_RESERVED_TO_COMMIT_ACK_MARKER_BUFFER (PRIVATE_BASE + 90)

#endif
