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

#include "priv.h"

const char mysqlmanager_version[] = "0.2-alpha";

const int mysqlmanager_version_length= sizeof(mysqlmanager_version) - 1;

const unsigned char protocol_version= PROTOCOL_VERSION;

unsigned long net_buffer_length= 16384;

unsigned long max_allowed_packet= 16384;

unsigned long net_read_timeout= 30;             // same as in mysqld

unsigned long net_write_timeout= 60;            // same as in mysqld

unsigned long net_retry_count= 10;              // same as in mysqld

