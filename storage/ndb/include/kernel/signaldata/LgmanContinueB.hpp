/*
   Copyright (c) 2005, 2013, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef LGMAN_CONTINUEB_H
#define LGMAN_CONTINUEB_H

#include "SignalData.hpp"

#define JAM_FILE_ID 54


struct LgmanContinueB {

  enum {
    CUT_LOG_TAIL = 0
    ,FILTER_LOG = 1
    ,FLUSH_LOG = 2
    ,PROCESS_LOG_BUFFER_WAITERS = 3
    ,FIND_LOG_HEAD = 4
    ,EXECUTE_UNDO_RECORD = 5
    ,READ_UNDO_LOG = 6
    ,STOP_UNDO_LOG = 7
    ,PROCESS_LOG_SYNC_WAITERS = 8
    ,FORCE_LOG_SYNC = 9
    ,DROP_FILEGROUP = 10
  };
};


#undef JAM_FILE_ID

#endif
