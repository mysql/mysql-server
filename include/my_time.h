/* Copyright (C) 2004 MySQL AB & MySQL Finland AB & TCX DataKonsult AB

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

/*
  This is a private header of sql-common library, containing
  declarations for my_time.c
*/

#ifndef _my_time_h_
#define _my_time_h_
#include "my_global.h"
#include "mysql_time.h"

C_MODE_START

extern ulonglong log_10_int[20];

#define YY_PART_YEAR	   70

/* Flags to str_to_datetime */
#define TIME_FUZZY_DATE    1
#define TIME_DATETIME_ONLY 2

enum enum_mysql_timestamp_type
str_to_datetime(const char *str, uint length, MYSQL_TIME *l_time,
                uint flags, int *was_cut);

bool str_to_time(const char *str,uint length, MYSQL_TIME *l_time,
                 int *was_cut);

C_MODE_END

#endif /* _my_time_h_ */
