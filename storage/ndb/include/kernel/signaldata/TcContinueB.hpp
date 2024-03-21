/*
   Copyright (c) 2003, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef TC_CONTINUEB_H
#define TC_CONTINUEB_H

#include "SignalData.hpp"

#define JAM_FILE_ID 16

class TcContinueB {
  /**
   * Sender(s)/Reciver(s)
   */
  friend class Dbtc;

 private:
  enum {
    ZRETURN_FROM_QUEUED_DELIVERY = 1,
    ZCOMPLETE_TRANS_AT_TAKE_OVER = 2,
    ZCONTINUE_TIME_OUT_CONTROL = 3,
    ZNODE_TAKE_OVER_COMPLETED = 4,
    ZINITIALISE_RECORDS = 5,
    ZSEND_COMMIT_LOOP = 6,
    ZSEND_COMPLETE_LOOP = 7,
    ZHANDLE_FAILED_API_NODE = 8,
    ZTRANS_EVENT_REP = 9,
    ZABORT_BREAK = 10,
    ZABORT_TIMEOUT_BREAK = 11,
    ZCONTINUE_TIME_OUT_FRAG_CONTROL = 12,
    ZHANDLE_FAILED_API_NODE_REMOVE_MARKERS = 13,
    ZWAIT_ABORT_ALL = 14,
    ZCHECK_SCAN_ACTIVE_FAILED_LQH = 15,
    TRIGGER_PENDING = 17,
    DelayTCKEYCONF = 18,
    ZNF_CHECK_TRANSACTIONS = 19,
    ZSEND_FIRE_TRIG_REQ = 20,
    ZSTART_FRAG_SCANS = 21,
    ZSEND_FRAG_SCANS = 22,
    ZSCAN_FOR_READ_BACKUP = 25,
    ZTRANSIENT_POOL_STAT = 26,
    ZSHRINK_TRANSIENT_POOLS = 27
  };
};

#undef JAM_FILE_ID

#endif
