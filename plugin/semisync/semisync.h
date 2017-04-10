/* Copyright (C) 2007 Google Inc.
   Copyright (c) 2000, 2017, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */


#ifndef SEMISYNC_H
#define SEMISYNC_H

#include <my_thread.h>
#include <mysql/plugin.h>
#include <replication.h>

#include "log.h"                                /* sql_print_information */
#include "my_io.h"

typedef struct st_mysql_show_var SHOW_VAR;
typedef struct st_mysql_sys_var SYS_VAR;


/**
   This class is used to trace function calls and other process
   information
*/
class Trace {
public:
  static const unsigned long kTraceFunction;
  static const unsigned long kTraceGeneral;
  static const unsigned long kTraceDetail;
  static const unsigned long kTraceNetWait;

  unsigned long           trace_level_;                      /* the level for tracing */

  inline void function_enter(const char *func_name)
  {
    if (trace_level_ & kTraceFunction)
      sql_print_information("---> %s enter", func_name);
  }

  inline int  function_exit(const char *func_name, int exit_code)
  {
    if (trace_level_ & kTraceFunction)
      sql_print_information("<--- %s exit (%d)", func_name, exit_code);
    return exit_code;
  }

  inline bool function_exit(const char *func_name, bool exit_code)
  {
    if (trace_level_ & kTraceFunction)
      sql_print_information("<--- %s exit (%s)", func_name,
                            exit_code ? "True" : "False");
    return exit_code;
  }

  inline void function_exit(const char *func_name)
  {
    if (trace_level_ & kTraceFunction)
      sql_print_information("<--- %s exit", func_name);
  }

Trace()
    :trace_level_(0L)
  {}
  Trace(unsigned long trace_level)
    :trace_level_(trace_level)
  {}
};

/**
   Base class for semi-sync master and slave classes
*/
class ReplSemiSyncBase
  :public Trace {
public:
  static const unsigned char  kSyncHeader[2];     /* three byte packet header */

  /* Constants in network packet header. */
  static const unsigned char kPacketMagicNum;
  static const unsigned char kPacketFlagSync;
};

/* The layout of a semisync slave reply packet:
   1 byte for the magic num
   8 bytes for the binlog positon
   n bytes for the binlog filename, terminated with a '\0'
*/
#define REPLY_MAGIC_NUM_LEN 1
#define REPLY_BINLOG_POS_LEN 8
#define REPLY_BINLOG_NAME_LEN (FN_REFLEN + 1)
#define REPLY_MESSAGE_MAX_LENGTH \
  (REPLY_MAGIC_NUM_LEN + REPLY_BINLOG_POS_LEN + REPLY_BINLOG_NAME_LEN)
#define REPLY_MAGIC_NUM_OFFSET 0
#define REPLY_BINLOG_POS_OFFSET (REPLY_MAGIC_NUM_OFFSET + REPLY_MAGIC_NUM_LEN)
#define REPLY_BINLOG_NAME_OFFSET (REPLY_BINLOG_POS_OFFSET + REPLY_BINLOG_POS_LEN)

#endif /* SEMISYNC_H */
