/* Copyright (C) 2000 MySQL AB & MySQL Finland AB & TCX DataKonsult AB & Sasha

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

// Sasha Pachev <sasha@mysql.com> is currently in charge of this file

#include "mysql_priv.h"
#include "repl_failsafe.h"

RPL_STATUS rpl_status=RPL_NULL;
pthread_mutex_t LOCK_rpl_status;
pthread_cond_t COND_rpl_status;

const char *rpl_role_type[] = {"MASTER","SLAVE",NullS};
TYPELIB rpl_role_typelib = {array_elements(rpl_role_type)-1,"",
			    rpl_role_type};

const char* rpl_status_type[] = {"AUTH_MASTER","ACTIVE_SLAVE","IDLE_SLAVE",
				 "LOST_SOLDIER","TROOP_SOLDIER",
				 "RECOVERY_CAPTAIN","NULL",NullS};
TYPELIB rpl_status_typelib= {array_elements(rpl_status_type)-1,"",
			     rpl_status_type};

void change_rpl_status(RPL_STATUS from_status, RPL_STATUS to_status)
{
  pthread_mutex_lock(&LOCK_rpl_status);
  if (rpl_status == from_status || rpl_status == RPL_ANY)
    rpl_status = to_status;
  pthread_mutex_unlock(&LOCK_rpl_status);
}

