/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#ifndef TC_CONTINUEB_H
#define TC_CONTINUEB_H

#include "SignalData.hpp"

class TcContinueB {
  /**
   * Sender(s)/Reciver(s)
   */
  friend class Dbtc;
private:
  enum {
    ZRETURN_FROM_QUEUED_DELIVERY           = 1,
    ZCOMPLETE_TRANS_AT_TAKE_OVER           = 2,
    ZCONTINUE_TIME_OUT_CONTROL             = 3,
    ZNODE_TAKE_OVER_COMPLETED              = 4,
    ZINITIALISE_RECORDS                    = 5,
    ZSEND_COMMIT_LOOP                      = 6,
    ZSEND_COMPLETE_LOOP                    = 7,
    ZHANDLE_FAILED_API_NODE                = 8,
    ZTRANS_EVENT_REP                       = 9,
    ZABORT_BREAK                           = 10,
    ZABORT_TIMEOUT_BREAK                   = 11,
    ZCONTINUE_TIME_OUT_FRAG_CONTROL        = 12,
    ZHANDLE_FAILED_API_NODE_REMOVE_MARKERS = 13,
    ZWAIT_ABORT_ALL                        = 14,
    ZCHECK_SCAN_ACTIVE_FAILED_LQH          = 15,
    CHECK_WAIT_DROP_TAB_FAILED_LQH         = 16,
    TRIGGER_PENDING                        = 17,
    
    DelayTCKEYCONF = 18
  };
};

#endif
