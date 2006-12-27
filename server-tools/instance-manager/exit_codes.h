#ifndef INCLUDES_MYSQL_INSTANCE_MANAGER_EXIT_CODES_H
#define INCLUDES_MYSQL_INSTANCE_MANAGER_EXIT_CODES_H

/*
   Copyright (C) 2006 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

/*
  This file contains a list of exit codes, which are used when Instance
  Manager is working in user-management mode.
*/

const int ERR_OK = 0;

const int ERR_OUT_OF_MEMORY = 1;
const int ERR_INVALID_USAGE = 2;
const int ERR_INTERNAL_ERROR = 3;
const int ERR_IO_ERROR = 4;
const int ERR_PASSWORD_FILE_CORRUPTED = 5;
const int ERR_PASSWORD_FILE_DOES_NOT_EXIST = 6;

const int ERR_CAN_NOT_READ_USER_NAME = 10;
const int ERR_CAN_NOT_READ_PASSWORD = 11;
const int ERR_USER_ALREADY_EXISTS = 12;
const int ERR_USER_NOT_FOUND = 13;

#endif // INCLUDES_MYSQL_INSTANCE_MANAGER_EXIT_CODES_H
