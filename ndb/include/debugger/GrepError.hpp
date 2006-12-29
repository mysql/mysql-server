/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#ifndef GREP_ERROR_H
#define GREP_ERROR_H

#include <ndb_types.h>

/**
 * 
 */
class GrepError {
public:
  enum GE_Code {
    GE_NO_ERROR = 0,
    SUBSCRIPTION_ID_NOMEM = 1,
    SUBSCRIPTION_ID_NOT_FOUND = 2,
    SUBSCRIPTION_ID_NOT_UNIQUE = 3,
    SUBSCRIPTION_ID_SUMA_FAILED_CREATE = 4,
    SUBSCRIPTION_ID_ALREADY_EXIST = 5,
    COULD_NOT_ALLOCATE_MEM_FOR_SIGNAL = 6,
    NULL_VALUE = 7,
    SEQUENCE_ERROR = 8,
    NOSPACE_IN_POOL= 9,
    SUBSCRIPTION_NOT_FOUND = 10,

    NF_FakeErrorREF = 11,

    // Error that the user can get when issuing commands
    SUBSCRIPTION_NOT_STARTED = 100,
    START_OF_COMPONENT_IN_WRONG_STATE,
    START_ALREADY_IN_PROGRESS,
    ILLEGAL_STOP_EPOCH_ID,
    WRONG_NO_OF_SECTIONS,
    ILLEGAL_ACTION_WHEN_STOPPING, 
    ILLEGAL_USE_OF_COMMAND,  
    CHANNEL_NOT_STOPPABLE,

    // subscriber releated 20 - 30
    SUBSCRIBER_NOT_FOUND = 20,

    //SUMA specific  400 - 600
    SELECTED_TABLE_NOT_FOUND = 400,
    SELECTED_TABLE_ALREADY_ADDED = 401,
    
    //REP ERRORS starts at 1000
    REP_NO_CONNECTED_NODES = 1001,
    REP_DELETE_NEGATIVE_EPOCH = 1002,
    REP_DELETE_NONEXISTING_EPOCH = 1003,
    REP_APPLY_LOGRECORD_FAILED = 1012,
    REP_APPLY_METARECORD_FAILED = 1013,
    REP_APPLY_NONCOMPLETE_GCIBUFFER = 1004,
    REP_APPLY_NULL_GCIBUFFER = 1005,
    REP_APPLIER_START_TRANSACTION = 1006,
    REP_APPLIER_NO_TABLE = 1007,
    REP_APPLIER_NO_OPERATION = 1007,
    REP_APPLIER_EXECUTE_TRANSACTION = 1008,
    REP_APPLIER_CREATE_TABLE = 1009,
    REP_APPLIER_PREPARE_TABLE = 1010,
    REP_DISCONNECT = 1011,
    REQUESTOR_ILLEGAL_STATE_FOR_SLOWSTOP = 1200,
    REQUESTOR_ILLEGAL_STATE_FOR_FASTSTOP = 1201,
    REP_NOT_PROPER_TABLE = 1202,
    REP_TABLE_ALREADY_SELECTED = 1203,
    REP_TABLE_NOT_FOUND = 1204,

    NOT_YET_IMPLEMENTED,
    NO_OF_ERROR_CODES
  };
  
  struct ErrorDescription {
    GE_Code errCode;
    const char * name;
  };
  static const ErrorDescription errorDescriptions[];
  static const Uint32 noOfErrorDescs;
  static const char * getErrorDesc(GrepError::GE_Code err);
  
};

#endif
