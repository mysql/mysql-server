#ifdef HAVE_REPLICATION
#ifndef REPL_FAILSAFE_H
#define REPL_FAILSAFE_H

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

pthread_handler_decl(handle_failsafe_rpl,arg);
void change_rpl_status(RPL_STATUS from_status, RPL_STATUS to_status);
int find_recovery_captain(THD* thd, MYSQL* mysql);
int update_slave_list(MYSQL* mysql, MASTER_INFO* mi);

extern HASH slave_list;

bool load_master_data(THD* thd);
int connect_to_master(THD *thd, MYSQL* mysql, MASTER_INFO* mi);

bool show_new_master(THD* thd);
bool show_slave_hosts(THD* thd);
int translate_master(THD* thd, LEX_MASTER_INFO* mi, char* errmsg);
void init_slave_list();
void end_slave_list();
int register_slave(THD* thd, uchar* packet, uint packet_length);
void unregister_slave(THD* thd, bool only_mine, bool need_mutex);

#endif
#endif /* HAVE_REPLICATION */
