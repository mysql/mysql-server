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

#ifndef DIH_CONTINUEB_H
#define DIH_CONTINUEB_H

#include "SignalData.hpp"

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
    //ZREAD_TAB_DESCRIPTION   =  5,
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
    ZSTART_TAKE_OVER             = 29,
    ZCHECK_START_TAKE_OVER       = 30,
    ZTO_START_COPY_FRAG          = 31,
    ZINITIALISE_RECORDS          = 33,    
    ZINVALIDATE_NODE_LCP         = 34,
    ZSTART_PERMREQ_AGAIN         = 35,
    SwitchReplica                = 36,
    ZSEND_START_TO               = 37,
    ZSEND_ADD_FRAG               = 38,
    ZSEND_CREATE_FRAG            = 39,
    ZSEND_UPDATE_TO              = 40,
    ZSEND_END_TO                 = 41,

    WAIT_DROP_TAB_WRITING_TO_FILE = 42,
    CHECK_WAIT_DROP_TAB_FAILED_LQH = 43
  };
};

#endif
