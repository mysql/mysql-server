#ifndef REPL_FAILSAFE_H
#define REPL_FAILSAFE_H

#include "mysql.h"

typedef enum {RPL_AUTH_MASTER=0,RPL_ACTIVE_SLAVE,RPL_IDLE_SLAVE,
	      RPL_LOST_SOLDIER,RPL_TROOP_SOLDIER,
	      RPL_RECOVERY_CAPTAIN,RPL_NULL /* inactive */,
	      RPL_ANY /* wild card used by change_rpl_status */ } RPL_STATUS;
extern RPL_STATUS rpl_status;

extern pthread_mutex_t LOCK_rpl_status;
extern pthread_cond_t COND_rpl_status;
extern TYPELIB rpl_role_typelib, rpl_status_typelib;
extern uint rpl_recovery_rank;
extern const char* rpl_role_type[], *rpl_status_type[];

pthread_handler_decl(handle_failsafe_rpl,arg);
void change_rpl_status(RPL_STATUS from_status, RPL_STATUS to_status);
int find_recovery_captain(THD* thd, MYSQL* mysql);
int update_slave_list(MYSQL* mysql);
#endif
