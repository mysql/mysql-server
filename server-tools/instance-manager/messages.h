#ifndef INCLUDES_MYSQL_INSTANCE_MANAGER_MESSAGES_H
#define INCLUDES_MYSQL_INSTANCE_MANAGER_MESSAGES_H
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

#include "mysqld_error.h"
#include "mysql_manager_error.h"

const char *message(unsigned sql_errno);

const char *errno_to_sqlstate(unsigned sql_errno);

#endif // INCLUDES_MYSQL_INSTANCE_MANAGER_MESSAGES_H
