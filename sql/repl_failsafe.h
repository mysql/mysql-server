#ifndef REPL_FAILSAFE_H
#define REPL_FAILSAFE_H

typedef enum {RPL_AUTH_MASTER=0,RPL_ACTIVE_SLAVE,RPL_IDLE_SLAVE,
	      RPL_LOST_SOLDIER,RPL_TROOP_SOLDIER,
	      RPL_RECOVERY_CAPTAIN,RPL_NULL} RPL_STATUS;
extern RPL_STATUS rpl_status;

extern pthread_mutex_t LOCK_rpl_status;
extern pthread_cond_t COND_rpl_status;
extern TYPELIB rpl_role_typelib, rpl_status_typelib;
extern const char* rpl_role_type[], *rpl_status_type[];
#endif
