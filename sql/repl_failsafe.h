/* Copyright (C) 2001-2005 MySQL AB & Sasha

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#ifdef HAVE_REPLICATION

#include "mysql.h"
#include "my_sys.h"
#include "slave.h"

typedef enum {RPL_AUTH_MASTER=0,RPL_ACTIVE_SLAVE,RPL_IDLE_SLAVE,
	      RPL_LOST_SOLDIER,RPL_TROOP_SOLDIER,
	      RPL_RECOVERY_CAPTAIN,RPL_NULL /* inactive */,
	      RPL_ANY /* wild card used by change_rpl_status */ } RPL_STATUS;
extern RPL_STATUS rpl_status;

extern pthread_mutex_t LOCK_rpl_status;
extern pthread_cond_t COND_rpl_status;
extern TYPELIB rpl_role_typelib, rpl_status_typelib;
extern const char* rpl_role_type[], *rpl_status_type[];

pthread_handler_t handle_failsafe_rpl(void *arg);
void change_rpl_status(RPL_STATUS from_status, RPL_STATUS to_status);
int find_recovery_captain(THD* thd, MYSQL* mysql);
int update_slave_list(MYSQL* mysql, Master_info* mi);

extern HASH slave_list;

bool load_master_data(THD* thd);
int connect_to_master(THD *thd, MYSQL* mysql, Master_info* mi);

bool show_new_master(THD* thd);
bool show_slave_hosts(THD* thd);
int translate_master(THD* thd, LEX_MASTER_INFO* mi, char* errmsg);
void init_slave_list();
void end_slave_list();
int register_slave(THD* thd, uchar* packet, uint packet_length);
void unregister_slave(THD* thd, bool only_mine, bool need_mutex);

#endif /* HAVE_REPLICATION */
