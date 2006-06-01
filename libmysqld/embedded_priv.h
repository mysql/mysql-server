/* Copyright (C) 2000 MySQL AB & MySQL Finland AB & TCX DataKonsult AB

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

/* Prototypes for the embedded version of MySQL */

#include <my_global.h>
#include <mysql.h>
#include <mysql_embed.h>
#include <mysqld_error.h>
#include <my_pthread.h>

C_MODE_START
void lib_connection_phase(NET *net, int phase);
void init_embedded_mysql(MYSQL *mysql, int client_flag);
void *create_embedded_thd(int client_flag);
int check_embedded_connection(MYSQL *mysql, const char *db);
void free_old_query(MYSQL *mysql);
void embedded_get_error(MYSQL *mysql);
extern MYSQL_METHODS embedded_methods;
C_MODE_END
