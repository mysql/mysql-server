/******************************************************
Transaction system global type definitions

(c) 1996 Innobase Oy

Created 3/26/1996 Heikki Tuuri
*******************************************************/

#ifndef trx0types_h
#define trx0types_h

#include "lock0types.h"
#include "ut0byte.h"

/* prepare trx_t::id for being printed via printf(3) */
#define TRX_ID_PREP_PRINTF(id)	(ullint) ut_conv_dulint_to_longlong(id)

/* printf(3) format used for printing TRX_ID_PRINTF_PREP() */
#define TRX_ID_FMT		"%llX"

/* maximum length that a formatted trx_t::id could take, not including
the terminating '\0'. */
#define TRX_ID_MAX_LEN		17

/* Memory objects */
typedef struct trx_struct	trx_t;
typedef struct trx_sys_struct	trx_sys_t;
typedef struct trx_doublewrite_struct	trx_doublewrite_t;
typedef struct trx_sig_struct	trx_sig_t;
typedef struct trx_rseg_struct	trx_rseg_t;
typedef struct trx_undo_struct	trx_undo_t;
typedef struct trx_undo_arr_struct trx_undo_arr_t;
typedef struct trx_undo_inf_struct trx_undo_inf_t;
typedef struct trx_purge_struct	trx_purge_t;
typedef struct roll_node_struct	roll_node_t;
typedef struct commit_node_struct commit_node_t;
typedef struct trx_named_savept_struct trx_named_savept_t;

/* Rollback modes */
enum trx_rbmode {
	RB_NONE = 0,	/* no rollback */
	RB_NORMAL,	/* normal rollback */
	RB_RECOVERY,	/* rolling back an incomplete transaction,
			in crash recovery */
};

/* Transaction savepoint */
typedef struct trx_savept_struct trx_savept_t;
struct trx_savept_struct{
	dulint	least_undo_no;	/* least undo number to undo */
};

/* File objects */
typedef byte	trx_sysf_t;
typedef byte	trx_rsegf_t;
typedef byte	trx_usegf_t;
typedef byte	trx_ulogf_t;
typedef byte	trx_upagef_t;

/* Undo log record */
typedef	byte	trx_undo_rec_t;

#endif
