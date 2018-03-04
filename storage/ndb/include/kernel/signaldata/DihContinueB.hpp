/*
   Copyright (c) 2003, 2017, Oracle and/or its affiliates. All rights reserved.

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

#ifndef DIH_CONTINUEB_H
#define DIH_CONTINUEB_H

#include "SignalData.hpp"

#define JAM_FILE_ID 114


class DihContinueB {
  /**
   * Sender(s)/Reciver(s)
   */
  friend class Dbdih;
  friend bool printCONTINUEB_DBDIH(FILE * output, const Uint32 * theData,
				   Uint32 len, Uint16);
private:
  enum Type {
    ZPACK_TABLE_INTO_PAGES  =  1,
    ZPACK_FRAG_INTO_PAGES   =  2,
    ZREAD_PAGES_INTO_TABLE  =  3,
    ZREAD_PAGES_INTO_FRAG   =  4,
    // 5 no longer used
    ZCOPY_TABLE             =  6,
    ZCOPY_TABLE_NODE        =  7,
    ZSTART_FRAGMENT         =  8,
    ZCOMPLETE_RESTART       =  9,
    ZREAD_TABLE_FROM_PAGES  = 10,
    ZSR_PHASE2_READ_TABLE   = 11,
    ZCHECK_TC_COUNTER       = 12,
    ZCALCULATE_KEEP_GCI     = 13,
    ZSTORE_NEW_LCP_ID       = 14,
    ZTABLE_UPDATE           = 15,
    ZCHECK_LCP_COMPLETED    = 16,
    ZINIT_LCP               = 17,
    // 18 no longer used
    ZADD_TABLE_MASTER_PAGES = 19,
    ZDIH_ADD_TABLE_MASTER   = 20,
    ZADD_TABLE_SLAVE_PAGES  = 21,
    ZDIH_ADD_TABLE_SLAVE    = 22,
    ZSTART_GCP              = 23,
    ZCOPY_GCI               = 24,
    ZEMPTY_VERIFY_QUEUE     = 25,
    ZCHECK_GCP_STOP         = 26,
    ZREMOVE_NODE_FROM_TABLE      = 27,
    ZCOPY_NODE                   = 28,
    // 29,30 no longer used
    ZTO_START_COPY_FRAG          = 31,
    // 32 no longer used
    ZINITIALISE_RECORDS          = 33,    
    ZINVALIDATE_NODE_LCP         = 34,
    ZSTART_PERMREQ_AGAIN         = 35,
    SwitchReplica                = 36,
    ZSEND_START_TO               = 37,
    ZSEND_ADD_FRAG               = 38,
    // 39 no longer used
    ZSEND_UPDATE_TO              = 40,

    // 41 no longer used
    WAIT_DROP_TAB_WRITING_TO_FILE = 42,
    // 43 no longer used
    ZTO_START_FRAGMENTS = 44
    // 45 no longer used
    ,ZWAIT_OLD_SCAN = 46
    ,ZLCP_TRY_LOCK = 47
    // 48 no longer used
    ,ZTO_START_LOGGING = 49
    ,ZGET_TABINFO = 50
    ,ZGET_TABINFO_SEND = 51
    ,ZDEQUEUE_LCP_REP = 52
  };
};


#undef JAM_FILE_ID

#endif
