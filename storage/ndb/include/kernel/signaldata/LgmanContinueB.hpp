/*
   Copyright (c) 2005, 2017, Oracle and/or its affiliates. All rights reserved.

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
    ,LEVEL_REPORT_THREAD = 11
  };
};


#undef JAM_FILE_ID

#endif
