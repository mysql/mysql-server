/* Copyright (C) 2003 MySQL AB & MySQL Finland AB & TCX DataKonsult AB

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
#include "messages.h"

#include <my_global.h>
#include <mysql_com.h>

#include <assert.h>


static const char *mysqld_error_message(unsigned sql_errno)
{
  switch (sql_errno) {
  case ER_HANDSHAKE_ERROR:
    return "Bad handshake";
  case ER_OUT_OF_RESOURCES:
    return "Out of memory;  Check if mysqld or some other process"
           " uses all available memory. If not you may have to use"
           " 'ulimit' to allow mysqld to use more memory or you can"
           " add more swap space";
  case ER_ACCESS_DENIED_ERROR:
    return "Access denied. Bad username/password pair";
  case ER_NOT_SUPPORTED_AUTH_MODE:
    return "Client does not support authentication protocol requested by"
           " server; consider upgrading MySQL client";
  case ER_UNKNOWN_COM_ERROR:
    return "Unknown command";
  case ER_SYNTAX_ERROR:
    return "You have an error in your command syntax. Check the manual that"
           " corresponds to your MySQL Instance Manager version for the right"
           " syntax to use";
  case ER_BAD_INSTANCE_NAME:
    return "Bad instance name. Check that the instance with such a name exists";
  case ER_INSTANCE_IS_NOT_STARTED:
    return "Cannot stop instance. Perhaps the instance is not started or you"
           " have specified wrong username/password in the config file";
  case ER_INSTANCE_ALREADY_STARTED:
    return "The instance is already started";
  case ER_CANNOT_START_INSTANCE:
    return "Cannot start instance. Possible reasons are wrong instance options"
           " or resources shortage";
  case ER_STOP_INSTANCE:
    return "Cannot stop instance";
  default:
    DBUG_ASSERT(0);
    return 0;
  }
}


const char *message(unsigned sql_errno)
{
  return mysqld_error_message(sql_errno);
}


const char *errno_to_sqlstate(unsigned sql_errno)
{
  return mysql_errno_to_sqlstate(sql_errno);
}
