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

#ifndef _MINI_CLIENT_H
#define _MINI_CLIENT_H


MYSQL* STDCALL
mc_mysql_connect(MYSQL *mysql,const char *host, const char *user,
		   const char *passwd, const char *db,
		 uint port, const char *unix_socket,uint client_flag);

int STDCALL
mc_simple_command(MYSQL *mysql,enum enum_server_command command, const char *arg,
		  uint length, my_bool skipp_check);
void STDCALL
mc_mysql_close(MYSQL *mysql);

MYSQL * STDCALL
mc_mysql_init(MYSQL *mysql);

void STDCALL
mc_mysql_debug(const char *debug);

uint STDCALL
mc_net_safe_read(MYSQL *mysql);

char * STDCALL mc_mysql_error(MYSQL *mysql);
int STDCALL mc_mysql_errno(MYSQL *mysql);
my_bool STDCALL mc_mysql_reconnect(MYSQL* mysql);


#endif

