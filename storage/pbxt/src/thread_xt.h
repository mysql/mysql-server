/* Copyright (c) 2005 PrimeBase Technologies GmbH
 *
 * PrimeBase XT
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 * 2005-01-03	Paul McCullagh
 *
 * H&G2JCtL
 */

#ifndef __xt_thread_h__
#define __xt_thread_h__

#include <stdio.h>
#ifndef XT_WIN
#include <sys/param.h>
#endif
#include <setjmp.h>

#include "xt_defs.h"
#include "xt_errno.h"
#include "linklist_xt.h"
#include "memory_xt.h"
#include "xactlog_xt.h"
#include "datalog_xt.h"
#include "lock_xt.h"
#include "locklist_xt.h"

/*
 * -----------------------------------------------------------------------
 * Macros and defines
 */

#define XT_ERR_MSG_SIZE					(PATH_MAX + 200)

#ifdef DEBUG
#define ASSERT(expr)					((expr) ? TRUE : xt_assert(self, #expr, __FUNC__, __FILE__, __LINE__))
#else
#define ASSERT(expr)					((void) 0)
#endif

#ifdef DEBUG
#define ASSUME(expr)					((expr) ? TRUE : xt_assume(self, #expr, __FUNC__, __FILE__, __LINE__))
#else
#define ASSUME(expr)					((void) 0)
#endif

#ifdef DEBUG
#define ASSERT_NS(expr)					((expr) ? TRUE : xt_assert(NULL, #expr, __FUNC__, __FILE__, __LINE__))
#else
#define ASSERT_NS(expr)					((void) 0)
#endif

#define XT_THROW_ASSERTION(str)			xt_throw_assertion(self, __FUNC__, __FILE__, __LINE__, str)

/* Log levels */
#define XT_LOG_DEFAULT					-1
#define XT_LOG_PROTOCOL					0
#define XT_LOG_FATAL					1
#define XT_LOG_ERROR					2
#define XT_LOG_WARNING					3
#define XT_LOG_INFO						4
#define XT_LOG_TRACE					5

#define XT_PROTOCOL						self, "", NULL, 0, XT_LOG_PROTOCOL
#define XT_WARNING						self, "", NULL, 0, XT_LOG_WARNING
#define XT_INFO							self, "", NULL, 0, XT_LOG_INFO
#define XT_ERROR						self, "", NULL, 0, XT_LOG_ERROR
#define XT_TRACE						self, "", NULL, 0, XT_LOG_TRACE

#define XT_NT_PROTOCOL					NULL, "", NULL, 0, XT_LOG_PROTOCOL
#define XT_NT_WARNING					NULL, "", NULL, 0, XT_LOG_WARNING
#define XT_NT_INFO						NULL, "", NULL, 0, XT_LOG_INFO
#define XT_NT_ERROR						NULL, "", NULL, 0, XT_LOG_ERROR
#define XT_NT_TRACE						NULL, "", NULL, 0, XT_LOG_TRACE

#define XT_ERROR_CONTEXT(func)			self, __FUNC__, __FILE__, __LINE__, XT_LOG_ERROR

/* Thread types */
#define XT_THREAD_MAIN					0
#define XT_THREAD_WORKER				1

/* Thread Priorities: */
#define XT_PRIORITY_LOW					0
#define XT_PRIORITY_NORMAL				1
#define XT_PRIORITY_HIGH				2

#define XT_CONTEXT						self, __FUNC__, __FILE__, __LINE__
#define XT_NS_CONTEXT					NULL, __FUNC__, __FILE__, __LINE__
#define XT_REG_CONTEXT					__FUNC__, __FILE__, __LINE__

#define XT_MAX_JMP						20
#define XT_MAX_CALL_STACK				100						/* The number of functions recorded by enter_() and exit() */
#define XT_RES_STACK_SIZE				4000					/* The size of the stack resource stack in bytes. */
#define XT_MAX_RESOURCE_USAGE			5						/* The maximum number of temp slots used per routine. */
#define XT_CATCH_TRACE_SIZE				1024
#define XT_MAX_FUNC_NAME_SIZE			120
#define XT_SOURCE_FILE_NAME_SIZE		40
#define XT_THR_NAME_SIZE				80

#ifdef XT_THREAD_LOCK_INFO
#define xt_init_rwlock_with_autoname(a,b)	xt_init_rwlock(a,b,LOCKLIST_ARG_SUFFIX(b))
#else
#define xt_init_rwlock_with_autoname(a,b)	xt_init_rwlock(a,b)
#endif

typedef struct XTException {
	int						e_xt_err;									/* The XT error number (ALWAYS non-zero on error, else zero) */
	int						e_sys_err;									/* The system error number (0 if none) */
	char					e_err_msg[XT_ERR_MSG_SIZE];					/* The error message text (0 terminated string) */
	char					e_func_name[XT_MAX_FUNC_NAME_SIZE];			/* The name of the function in which the exception occurred */
	char					e_source_file[XT_SOURCE_FILE_NAME_SIZE];	/* The source file in which the exception was thrown */
	u_int					e_source_line;								/* The source code line number on which the exception was thrown */
	char					e_catch_trace[XT_CATCH_TRACE_SIZE];			/* A string of the catch trace. */
} XTExceptionRec, *XTExceptionPtr;

struct XTThread;
struct XTSortedList;
struct XTXactLog;
struct XTXactData;
struct XTDatabase;

typedef void (*XTThreadFreeFunc)(struct XTThread *self, void *data);

typedef struct XTResourceArgs {
	void					*ra_p1;
	xtWord4					ra_p2;
} XTResourceArgsRec, *XTResourceArgsPtr;

/* This structure represents a temporary resource on the resource stack.
 * Resource are automatically freed if an exception occurs.
 */
typedef struct XTResource {
	xtWord4					r_prev_size;					/* The size of the previous resource on the stack (must be first!) */
	void					*r_data;						/* A pointer to the resource data (this may be on the resource stack) */
	XTThreadFreeFunc		r_free_func;					/* The function used to free the resource. */
} XTResourceRec, *XTResourcePtr;

typedef struct XTJumpBuf {
	XTResourcePtr			jb_res_top;
	int						jb_call_top;
	jmp_buf					jb_buffer;
} XTJumpBufRec, *XTJumpBufPtr;

typedef struct XTCallStack {
	c_char					*cs_func;
	c_char					*cs_file;
	u_int					cs_line;
} XTCallStackRec, *XTCallStackPtr;

typedef struct XTIOStats {
	u_int					ts_read;						/* The number of bytes read. */
	u_int					ts_write;						/* The number of bytes written. */
	xtWord8					ts_flush_time;					/* The accumulated flush time. */
	xtWord8					ts_flush_start;					/* Start time, non-zero if a timer is running. */
	u_int					ts_flush;						/* The number of flush operations. */
} XTIOStatsRec, *XTIOStatsPtr;

#define XT_ADD_STATS(x, y)	{ \
	(x).ts_read += (y).ts_read; \
	(x).ts_write += (y).ts_write; \
	(x).ts_flush_time += (y).ts_flush_time; \
	(x).ts_flush += (y).ts_flush; \
}

typedef struct XTStatistics {
	u_int					st_commits;
	u_int					st_rollbacks;
	u_int					st_stat_read;
	u_int					st_stat_write;

	XTIOStatsRec			st_rec;
	u_int					st_rec_cache_hit;
	u_int					st_rec_cache_miss;
	u_int					st_rec_cache_frees;

	XTIOStatsRec			st_ind;
	u_int					st_ind_cache_hit;
	u_int					st_ind_cache_miss;
	XTIOStatsRec			st_ilog;

	XTIOStatsRec			st_xlog;
	u_int					st_xlog_cache_hit;
	u_int					st_xlog_cache_miss;

	XTIOStatsRec			st_data;

	XTIOStatsRec			st_x;

	u_int					st_scan_index;
	u_int					st_scan_table;
	u_int					st_row_select;
	u_int					st_row_insert;
	u_int					st_row_update;
	u_int					st_row_delete;

	u_int					st_wait_for_xact;
	u_int					st_retry_index_scan;
	u_int					st_reread_record_list;
	XTIOStatsRec			st_ind_flush_time;
} XTStatisticsRec, *XTStatisticsPtr;

/*
 * PBXT supports COMMITTED READ and REPEATABLE READ.
 *
 * As Jim says, multi-versioning cannot implement SERIALIZABLE. Basically
 * you need locking to do this. Although phantom reads do not occur with
 * MVCC, it is still not serializable.
 *
 * This can be seen from the following example:
 *
 * T1: INSERT t1 VALUE (1, 1);
 * T2: INSERT t1 VALUE (2, 2);
 * T1: UPDATE t1 SET b = 3 WHERE a IN (1, 2);
 * T2: UPDATE t1 SET b = 4 WHERE a IN (1, 2);
 * Serialized result (T1, T2) or (T2, T1):
 * a   b	or	a   b
 * 1   4		1   3
 * 2   4		1   3
 * Non-serialized (MVCC) result:
 * a   b
 * 1   3
 * 2   4
 */
#define XT_XACT_UNCOMMITTED_READ	0
#define XT_XACT_COMMITTED_READ		1
#define XT_XACT_REPEATABLE_READ		2						/* Guarentees rows already read will not change. */
#define XT_XACT_SERIALIZABLE		3						

typedef struct XTThread {
	XTLinkedItemRec			t_links;						/* Required to be a member of a double-linked list. */

	char					t_name[XT_THR_NAME_SIZE];		/* The name of the thread. */
	xtBool					t_main;							/* TRUE if this is the main (initial) thread */
	xtBool					t_quit;							/* TRUE if this thread should stop running. */
	xtBool					t_daemon;						/* TRUE if this thread is a daemon. */
	xtThreadID				t_id;							/* The thread ID (0=main), index into thread array. */
	pthread_t				t_pthread;						/* The pthread associated with xt thread */
	xtBool					t_disable_interrupts;			/* TRUE if interrupts are disabled. */
	int						t_delayed_signal;				/* Throw this signal as soon as you can! */

	void					*t_data;						/* Data passed to the thread. */
	XTThreadFreeFunc		t_free_data;					/* Routine used to free the thread data */

	int						t_call_top;						/* A pointer to the top of the call stack. */
	XTCallStackRec			t_call_stack[XT_MAX_CALL_STACK];/* Records the function under execution (to be output on error). */

	XTResourcePtr			t_res_top;						/* The top of the resource stack (reference next free space). */
	union {
		char				t_res_stack[XT_RES_STACK_SIZE];	/* Temporary data to be freed if an exception occurs. */
		xtWord4				t_align_res_stack;
	} x;

	int						t_jmp_depth;					/* The current jump depth */
	XTJumpBufRec			t_jmp_env[XT_MAX_JMP];			/* The process environment to be restored on exception */
	XTExceptionRec			t_exception;					/* The exception details. */

	xt_cond_type			t_cond;							/* The pthread condition used for suspending the thread. */
	xt_mutex_type			t_lock;							/* Thread lock, used for operations on a thread that may be done by other threads.
															 * for example xt_unuse_database().
															 */
	
	/* Application specific data: */
	struct XTDatabase		*st_database;					/* The database in use by the thread. */
	u_int					st_lock_count;					/* We count the number of locks MySQL has set in order to know when they are all released. */
	u_int					st_stat_count;					/* start statement count. */
	struct XTXactData		*st_xact_data;					/* The transaction data, not NULL if the transaction performs an update. */
	xtBool					st_xact_writer;					/* TRUE if the transaction has written somthing to the log. */
	time_t					st_xact_write_time;				/* Approximate first write time (uses xt_db_approximate_time). */
	xtBool					st_xact_long_running;			/* TRUE if this is a long running writer transaction. */
	xtWord4					st_visible_time;				/* Transactions committed before this time are visible. */
	XTDataLogBufferRec		st_dlog_buf;
	
	/* A list of the last 10 transactions run by this connection: */
#ifdef XT_WAIT_FOR_CLEANUP
	u_int					st_last_xact;
	xtXactID				st_prev_xact[XT_MAX_XACT_BEHIND];
#endif

	int						st_xact_mode;					/* The transaction mode. */
	xtBool					st_ignore_fkeys;				/* TRUE if we must ignore foreign keys. */
	xtBool					st_auto_commit;					/* TRUE if this is an auto-commit transaction. */
	xtBool					st_table_trans;					/* TRUE transactions is a result of LOCK TABLES. */
	xtBool					st_abort_trans;					/* TRUE if the transaction should be aborted. */
	xtBool					st_stat_ended;					/* TRUE if the statement was ended. */
	xtBool					st_stat_trans;					/* TRUE if a statement transaction is running (started on UPDATE). */
	xtBool					st_stat_modify;					/* TRUE if the statement is an INSERT/UPDATE/DELETE */
#ifdef XT_IMPLEMENT_NO_ACTION
	XTBasicListRec			st_restrict_list;				/* These records have been deleted and should have no reference. */
#endif
	/* Local thread list. */
	u_int					st_thread_list_count;
	u_int					st_thread_list_size;
	xtThreadID				*st_thread_list;

	/* Used to prevent a record from being updated twice in one statement. */
	xtBool					st_is_update;					/* TRUE if this is an UPDATE statement. */
	u_int					st_update_id;					/* The update statement ID. */

	XTRowLockListRec		st_lock_list;					/* The thread row lock list (drop locks on transaction end). */
	XTStatisticsRec			st_statistics;					/* Accumulated statistics for this thread. */
#ifdef XT_THREAD_LOCK_INFO
	/* list of locks (spins, mutextes, etc) that this thread currently holds (debugging) */
	XTThreadLockInfoPtr		st_thread_lock_list[XT_THREAD_LOCK_INFO_MAX_COUNT];
	int						st_thread_lock_count;
#endif
} XTThreadRec, *XTThreadPtr;

/*
 * -----------------------------------------------------------------------
 * Call stack
 */

#define XT_INIT_CHECK_STACK		char xt_chk_buffer[512]; memset(xt_chk_buffer, 0xFE, 512);
#define XT_RE_CHECK_STACK		memset(xt_chk_buffer, 0xFE, 512);

/*
 * This macro must be placed at the start of every function.
 * It records the current context so that we can
 * dump a type of stack trace later if necessary.
 *
 * It also sets up the current thread pointer 'self'.
 */
#ifdef DEBUG
#define XT_STACK_TRACE
#endif

/*
 * These macros generate a stack trace which can be used
 * to locate an error on exception.
 */
#ifdef XT_STACK_TRACE

/*
 * Place this call at the top of a function,
 * after the declaration of local variable, and
 * before the first code is executed.
 */
#define enter_()			int xt_frame = self->t_call_top++; \
							do { \
								if (xt_frame < XT_MAX_CALL_STACK) { \
									self->t_call_stack[xt_frame].cs_func = __FUNC__; \
									self->t_call_stack[xt_frame].cs_file = __FILE__; \
									self->t_call_stack[xt_frame].cs_line = __LINE__; \
								} \
							} while (0)

#define outer_()			self->t_call_top = xt_frame;

/*
 * On exit to a function, either exit_() or
 * return_() must be called.
 */
#define exit_()				do { \
								outer_(); \
								return; \
							} while (0)
	
#define return_(x)			do { \
								outer_(); \
								return(x); \
							} while (0)

#define returnc_(x, typ)	do { \
								typ rv; \
								rv = (x); \
								outer_(); \
								return(rv); \
							} while (0)

/*
 * Sets the line number before a call to get a better
 * stack trace;
 */
#define call_(x)			do { self->t_call_stack[xt_frame].cs_line = __LINE__; x; } while (0)

#else
#define enter_()
#define outer_()
#define exit_()				return;
#define return_(x)			return (x)
#define returnc_(x, typ)	return (x)
#define call_(x)			x
#endif

/*
 * -----------------------------------------------------------------------
 * Throwing and catching
 */

int prof_setjmp(void);

#define TX_CHK_JMP()		if ((self)->t_jmp_depth < 0 || (self)->t_jmp_depth >= XT_MAX_JMP) xt_throw_xterr(self, __FUNC__, __FILE__, __LINE__, XT_ERR_JUMP_OVERFLOW)
#ifdef PROFILE
#define profile_setjmp		prof_setjmp()
#else
#define profile_setjmp			
#endif

#define try_(n)				TX_CHK_JMP(); \
							(self)->t_jmp_env[(self)->t_jmp_depth].jb_res_top = (self)->t_res_top; \
							(self)->t_jmp_env[(self)->t_jmp_depth].jb_call_top = (self)->t_call_top; \
							(self)->t_jmp_depth++; profile_setjmp; if (setjmp((self)->t_jmp_env[(self)->t_jmp_depth-1].jb_buffer)) goto catch_##n;
#define catch_(n)			(self)->t_jmp_depth--; goto cont_##n; catch_##n: (self)->t_jmp_depth--; xt_caught(self);
#define cont_(n)			cont_##n:
#define throw_()			xt_throw(self)

/*
 * -----------------------------------------------------------------------
 * Resource stack
 */

//#define DEBUG_RESOURCE_STACK

#ifdef DEBUG_RESOURCE_STACK
#define CHECK_RS			if ((char *) (self)->t_res_top < (self)->x.t_res_stack) xt_bug(self);
#define CHECK_NS_RS			{ XTThreadPtr self = xt_get_self(); CHECK_RS; }
#else
#define CHECK_RS			remove this!
#define CHECK_NS_RS			remove this!
#endif

/*
 * Allocate a resource on the resource stack. The resource will be freed
 * automatocally if an exception occurs. Before exiting the current
 * procedure you must free the resource using popr_() or freer_().
 * v = value to be set to the resource,
 * f = function which frees the resource,
 * s = the size of the resource,
 */

/* GOTCHA: My experience is that contructs such as *((xtWordPS *) &(v)) = (xtWordPS) (x)
 * cause optimised versions to crash?!
 */
#define allocr_(v, f, s, t)		do { \
									if (((char *) (self)->t_res_top) > (self)->x.t_res_stack + XT_RES_STACK_SIZE - sizeof(XTResourceRec) + (s) + 4) \
										xt_throw_xterr(self, __FUNC__, __FILE__, __LINE__, XT_ERR_RES_STACK_OVERFLOW); \
									v = (t) (((char *) (self)->t_res_top) + sizeof(XTResourceRec)); \
									(self)->t_res_top->r_data = (v); \
									(self)->t_res_top->r_free_func = (XTThreadFreeFunc) (f); \
									(self)->t_res_top = (XTResourcePtr) (((char *) (self)->t_res_top) + sizeof(XTResourceRec) + (s)); \
									(self)->t_res_top->r_prev_size = sizeof(XTResourceRec) + (s); \
								} while (0)

#define alloczr_(v, f, s, t)	do { allocr_(v, f, s, t); \
									memset(v, 0, s); } while (0)

/* Push and set a resource:
 * v = value to be set to the resource,
 * f = function which frees the resource,
 * r = the resource,
 * NOTE: the expression (r) must come first because it may contain
 * calls which use the resource stack!!
 */
#define pushsr_(v, f, r)	do { \
								if (((char *) (self)->t_res_top) > (self)->x.t_res_stack + XT_RES_STACK_SIZE - sizeof(XTResourceRec) + 4) \
									xt_throw_xterr(self, __FUNC__, __FILE__, __LINE__, XT_ERR_RES_STACK_OVERFLOW); \
								v = (r); \
								(self)->t_res_top->r_data = (v); \
								(self)->t_res_top->r_free_func = (XTThreadFreeFunc) (f); \
								(self)->t_res_top = (XTResourcePtr) (((char *) (self)->t_res_top) + sizeof(XTResourceRec)); \
								(self)->t_res_top->r_prev_size = sizeof(XTResourceRec); \
							} while (0)

/* Push a resource. In the event of an exception it will be freed
 * the free routine.
 * f = function which frees the resource,
 * r = a pointer to the resource,
 */
#define pushr_(f, r)		do { \
								if (((char *) (self)->t_res_top) > (self)->x.t_res_stack + XT_RES_STACK_SIZE - sizeof(XTResourceRec) + 4) \
									xt_throw_xterr(self, __FUNC__, __FILE__, __LINE__, XT_ERR_RES_STACK_OVERFLOW); \
								(self)->t_res_top->r_data = (r); \
								(self)->t_res_top->r_free_func = (XTThreadFreeFunc) (f); \
								(self)->t_res_top = (XTResourcePtr) (((char *) (self)->t_res_top) + sizeof(XTResourceRec)); \
								(self)->t_res_top->r_prev_size = sizeof(XTResourceRec); \
							} while (0)

/* Pop a resource without freeing it: */
#ifdef DEBUG_RESOURCE_STACK
#define popr_()				do { \
								(self)->t_res_top = (XTResourcePtr) (((char *) (self)->t_res_top) - (self)->t_res_top->r_prev_size); \
								if ((char *) (self)->t_res_top < (self)->x.t_res_stack) \
									xt_bug(self); \
							} while (0)
#else
#define popr_()				do { (self)->t_res_top = (XTResourcePtr) (((char *) (self)->t_res_top) - (self)->t_res_top->r_prev_size); } while (0)
#endif

#define setr_(r)			do { ((XTResourcePtr) (((char *) (self)->t_res_top) - (self)->t_res_top->r_prev_size))->r_data = (r); } while (0)

/* Pop and free a resource: */
#ifdef DEBUG_RESOURCE_STACK
#define freer_()			do {  \
								register XTResourcePtr	rp; \
								rp = (XTResourcePtr) (((char *) (self)->t_res_top) - (self)->t_res_top->r_prev_size); \
								if ((char *) rp < (self)->x.t_res_stack) \
									xt_bug(self); \
								(rp->r_free_func)((self), rp->r_data); \
								(self)->t_res_top = rp; \
							} while (0)
#else
#define freer_()			do {  \
								register XTResourcePtr	rp; \
								rp = (XTResourcePtr) (((char *) (self)->t_res_top) - (self)->t_res_top->r_prev_size); \
								(rp->r_free_func)((self), rp->r_data); \
								(self)->t_res_top = rp; \
							} while (0)
#endif

/*
 * -----------------------------------------------------------------------
 * Thread globals
 */

extern u_int			xt_thr_maximum_threads;
extern u_int			xt_thr_current_thread_count;
extern u_int			xt_thr_current_max_threads;
extern struct XTThread	**xt_thr_array;

/*
 * -----------------------------------------------------------------------
 * Function prototypes
 */

void			xt_get_now(char *buffer, size_t len);
xtBool			xt_init_logging(void);
void			xt_exit_logging(void);
void			xt_log_flush(XTThreadPtr self);
void			xt_logf(XTThreadPtr self, c_char *func, c_char *file, u_int line, int level, c_char *fmt, ...);
void			xt_log(XTThreadPtr self, c_char *func, c_char *file, u_int line, int level, c_char *string);
int				xt_log_errorf(XTThreadPtr self, c_char *func, c_char *file, u_int line, int level, int xt_err, int sys_err, c_char *fmt, ...);
int				xt_log_error(XTThreadPtr self, c_char *func, c_char *file, u_int line, int level, int xt_err, int sys_err, c_char *string);
void			xt_log_exception(XTThreadPtr self, XTExceptionPtr e, int level);
void			xt_clear_exception(XTThreadPtr self);
void			xt_log_and_clear_exception(XTThreadPtr self);
void			xt_log_and_clear_exception_ns(void);
void			xt_log_and_clear_warning(XTThreadPtr self);
void			xt_log_and_clear_warning_ns(void);

void			xt_bug(XTThreadPtr self);
void			xt_caught(XTThreadPtr self);
void			xt_throw(XTThreadPtr self);
void			xt_throwf(XTThreadPtr self, c_char *func, c_char *file, u_int line, int xt_err, int sys_err, c_char *format, ...);
void			xt_throw_error(XTThreadPtr self, c_char *func, c_char *file, u_int line, int xt_err, int sys_err, c_char *message);
void			xt_throw_i2xterr(XTThreadPtr self, c_char *func, c_char *file, u_int line, int xt_err, c_char *item, c_char *item2);
void			xt_throw_ixterr(XTThreadPtr self, c_char *func, c_char *file, u_int line, int xt_err, c_char *item);
void			xt_throw_tabcolerr(XTThreadPtr self, c_char *func, c_char *file, u_int line, int xt_err, XTPathStrPtr tab_item, c_char *item2);
void			xt_throw_taberr(XTThreadPtr self, c_char *func, c_char *file, u_int line, int xt_err, XTPathStrPtr tab_item);
void			xt_throw_ulxterr(XTThreadPtr self, c_char *func, c_char *file, u_int line, int xt_err, u_long value);
void			xt_throw_sulxterr(XTThreadPtr self, c_char *func, c_char *file, u_int line, int xt_err, c_char *item, u_long value);
void			xt_throw_xterr(XTThreadPtr self, c_char *func, c_char *file, u_int line, int xt_err);
void			xt_throw_errno(XTThreadPtr self, c_char *func, c_char *file, u_int line, int err_no);
void			xt_throw_ferrno(XTThreadPtr self, c_char *func, c_char *file, u_int line, int err_no, c_char *path);
void			xt_throw_assertion(XTThreadPtr self, c_char *func, c_char *file, u_int line, c_char *str);
void			xt_throw_signal(XTThreadPtr self, c_char *func, c_char *file, u_int line, int sig);
xtBool			xt_throw_delayed_signal(XTThreadPtr self, c_char *func, c_char *file, u_int line);

void			xt_registerf(c_char *func, c_char *file, u_int line, int xt_err, int sys_err, c_char *fmt, ...);
void			xt_register_i2xterr(c_char *func, c_char *file, u_int line, int xt_err, c_char *item, c_char *item2);
void			xt_register_ixterr(c_char *func, c_char *file, u_int line, int xt_err, c_char *item);
void			xt_register_tabcolerr(c_char *func, c_char *file, u_int line, int xt_err, XTPathStrPtr tab_item, c_char *item2);
void			xt_register_taberr(c_char *func, c_char *file, u_int line, int xt_err, XTPathStrPtr tab_item);
void			xt_register_ulxterr(c_char *func, c_char *file, u_int line, int xt_err, u_long value);
xtBool			xt_register_ferrno(c_char *func, c_char *file, u_int line, int err, c_char *path);
void			xt_register_error(c_char *func, c_char *file, u_int line, int xt_err, int sys_err, c_char *msg);
xtBool			xt_register_errno(c_char *func, c_char *file, u_int line, int err);
void			xt_register_xterr(c_char *func, c_char *file, u_int line, int xt_err);

void			xt_exceptionf(XTExceptionPtr e, XTThreadPtr self, c_char *func, c_char *file, u_int line, int xt_err, int sys_err, c_char *fmt, ...);
void			xt_exception_error(XTExceptionPtr e, XTThreadPtr self, c_char *func, c_char *file, u_int line, int xt_err, int sys_err, c_char *msg);
xtBool			xt_exception_errno(XTExceptionPtr e, XTThreadPtr self, c_char *func, c_char *file, u_int line, int err);

void			xt_log_errno(XTThreadPtr self, c_char *func, c_char *file, u_int line, int err);

xtBool			xt_assert(XTThreadPtr self, c_char *expr, c_char *func, c_char *file, u_int line);
xtBool			xt_assume(XTThreadPtr self, c_char *expr, c_char *func, c_char *file, u_int line);

XTThreadPtr		xt_init_threading(u_int max_threads);
void			xt_exit_threading(XTThreadPtr self);

XTThreadPtr		xt_create_thread(c_char *name, xtBool main_thread, xtBool temp_thread, XTExceptionPtr e);
XTThreadPtr		xt_create_daemon(XTThreadPtr parent, c_char *name);
void			xt_free_thread(XTThreadPtr self);
void			xt_set_thread_data(XTThreadPtr self, void *data, XTThreadFreeFunc free_func);
pthread_t		xt_run_thread(XTThreadPtr parent, XTThreadPtr child, void *(*start_routine)(XTThreadPtr));
void			xt_exit_thread(XTThreadPtr self, void *result);
void			*xt_wait_for_thread(xtThreadID tid, xtBool ignore_error);
void			xt_signal_all_threads(XTThreadPtr self, int sig);
void			xt_do_to_all_threads(XTThreadPtr self, void (*do_func_ptr)(XTThreadPtr self, XTThreadPtr to_thr, void *thunk), void *thunk);
void			xt_kill_thread(pthread_t t1);
XTThreadPtr		xt_get_self(void);
void			xt_set_self(XTThreadPtr self);
void			xt_wait_for_all_threads(XTThreadPtr self);
void			xt_busy_wait(void);
void			xt_critical_wait(void);
void			xt_yield(void);
void			xt_sleep_milli_second(u_int t);
xtBool 			xt_suspend(XTThreadPtr self);
xtBool 			xt_unsuspend(XTThreadPtr self, XTThreadPtr target);
void			xt_lock_thread(XTThreadPtr thread);
void			xt_unlock_thread(XTThreadPtr thread);
xtBool			xt_wait_thread(XTThreadPtr thread);
void			xt_signal_thread(XTThreadPtr target);
void 			xt_terminate_thread(XTThreadPtr self, XTThreadPtr target);
xtProcID		xt_getpid();
xtBool			xt_process_exists(xtProcID pid);

#ifdef XT_THREAD_LOCK_INFO
#define	xt_init_rwlock_with_autoname(a,b) xt_init_rwlock(a,b,LOCKLIST_ARG_SUFFIX(b))
xtBool xt_init_rwlock(XTThreadPtr self, xt_rwlock_type *rwlock, const char *name);
#else
#define	xt_init_rwlock_with_autoname(a,b) xt_init_rwlock(a,b)
xtBool xt_init_rwlock(XTThreadPtr self, xt_rwlock_type *rwlock);
#endif

void			xt_free_rwlock(xt_rwlock_type *rwlock);
xt_rwlock_type	*xt_slock_rwlock(XTThreadPtr self, xt_rwlock_type *rwlock);
xt_rwlock_type	*xt_xlock_rwlock(XTThreadPtr self, xt_rwlock_type *rwlock);
void			xt_unlock_rwlock(XTThreadPtr self, xt_rwlock_type *rwlock);

xt_mutex_type	*xt_new_mutex(XTThreadPtr self);
void			xt_delete_mutex(XTThreadPtr self, xt_mutex_type *mx);
#ifdef XT_THREAD_LOCK_INFO
#define			xt_init_mutex_with_autoname(a,b) xt_init_mutex(a,b,LOCKLIST_ARG_SUFFIX(b))
xtBool			xt_init_mutex(XTThreadPtr self, xt_mutex_type *mx, const char *name);
#else
#define			xt_init_mutex_with_autoname(a,b) xt_init_mutex(a,b)
xtBool			xt_init_mutex(XTThreadPtr self, xt_mutex_type *mx);
#endif
void			xt_free_mutex(xt_mutex_type *mx);
xtBool			xt_lock_mutex(XTThreadPtr self, xt_mutex_type *mx);
void			xt_unlock_mutex(XTThreadPtr self, xt_mutex_type *mx);

pthread_cond_t	*xt_new_cond(XTThreadPtr self);
void			xt_delete_cond(XTThreadPtr self, pthread_cond_t *cond);

xtBool			xt_init_cond(XTThreadPtr self, pthread_cond_t *cond);
void			xt_free_cond(pthread_cond_t *cond);
xtBool			xt_wait_cond(XTThreadPtr self, pthread_cond_t *cond, xt_mutex_type *mutex);
xtBool			xt_timed_wait_cond(XTThreadPtr self, pthread_cond_t *cond, xt_mutex_type *mutex, u_long milli_sec);
xtBool			xt_signal_cond(XTThreadPtr self, pthread_cond_t *cond);
void			xt_broadcast_cond(XTThreadPtr self, pthread_cond_t *cond);
xtBool			xt_broadcast_cond_ns(xt_cond_type *cond);

xtBool			xt_set_key(pthread_key_t key, const void *value, XTExceptionPtr e);
void			*xt_get_key(pthread_key_t key);

void			xt_set_low_priority(XTThreadPtr self);
void			xt_set_normal_priority(XTThreadPtr self);
void			xt_set_high_priority(XTThreadPtr self);
void			xt_set_priority(XTThreadPtr self, int priority);

void			xt_gather_statistics(XTStatisticsPtr stats);
u_llong			xt_get_statistic(XTStatisticsPtr stats, struct XTDatabase *db, u_int rec_id);

#define xt_timed_wait_cond_ns(a, b, c)	xt_timed_wait_cond(NULL, a, b, c)

#endif

