/* Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License, version 2.0,
as published by the Free Software Foundation.

This program is also distributed with certain software (including
but not limited to OpenSSL) that is licensed under separate terms,
as designated in a particular file or component or in included license
documentation.  The authors of MySQL hereby grant you an additional
permission to link the program and your derivative works with the
separately licensed software that they have included with MySQL.

Without limiting anything contained in the foregoing, this file,
which is part of C Driver for MySQL (Connector/C), is also subject to the
Universal FOSS Exception, version 1.0, a copy of which can be found at
http://oss.oracle.com/licenses/universal-foss-exception.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License, version 2.0, for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef _mysql_command_h
#define _mysql_command_h

/**
  @enum  enum_server_command
  @brief You should add new commands to the end of this list, otherwise old
         servers won't be able to handle them as 'unsupported'.
*/
enum enum_server_command
{
  COM_SLEEP,
  COM_QUIT,
  COM_INIT_DB,
  COM_QUERY,
  COM_FIELD_LIST,
  COM_CREATE_DB,
  COM_DROP_DB,
  COM_REFRESH,
  COM_SHUTDOWN,
  COM_STATISTICS,
  COM_PROCESS_INFO,
  COM_CONNECT,
  COM_PROCESS_KILL,
  COM_DEBUG,
  COM_PING,
  COM_TIME,
  COM_DELAYED_INSERT,
  COM_CHANGE_USER,
  COM_BINLOG_DUMP,
  COM_TABLE_DUMP,
  COM_CONNECT_OUT,
  COM_REGISTER_SLAVE,
  COM_STMT_PREPARE,
  COM_STMT_EXECUTE,
  COM_STMT_SEND_LONG_DATA,
  COM_STMT_CLOSE,
  COM_STMT_RESET,
  COM_SET_OPTION,
  COM_STMT_FETCH,
  COM_DAEMON,
  COM_BINLOG_DUMP_GTID,
  COM_RESET_CONNECTION,
  /* don't forget to update const char *command_name[] in sql_parse.cc */

  /* Must be last */
  COM_END
};

#endif /* _mysql_command_h */
