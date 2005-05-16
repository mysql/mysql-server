#ifndef INCLUDES_MYSQL_INSTANCE_MANAGER_MYSQL_MANAGER_ERROR_H
#define INCLUDES_MYSQL_INSTANCE_MANAGER_MYSQL_MANAGER_ERROR_H
/* Copyright (C) 2004 MySQL AB

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

/* Definefile for instance manager error messagenumbers */

#define ER_BAD_INSTANCE_NAME        3000
#define ER_INSTANCE_IS_NOT_STARTED  3001
#define ER_INSTANCE_ALREADY_STARTED 3002
#define ER_CANNOT_START_INSTANCE    3003
#define ER_STOP_INSTANCE            3004
#define ER_NO_SUCH_LOG              3005
#define ER_OPEN_LOGFILE             3006
#define ER_GUESS_LOGFILE            3007
#define ER_ACCESS_OPTION_FILE       3008
#define ER_OFFSET_ERROR             3009
#define ER_READ_FILE                3010

#endif /* INCLUDES_MYSQL_INSTANCE_MANAGER_MYSQL_MANAGER_ERROR_H */
