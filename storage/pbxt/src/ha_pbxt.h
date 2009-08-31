/* Copyright (c) 2005 PrimeBase Technologies GmbH
 *
 * Derived from ha_example.h
 * Copyright (C) 2003 MySQL AB
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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA	02111-1307	USA
 *
 * 2005-11-10	Paul McCullagh
 *
 */
#ifndef __ha_pbxt_h__
#define __ha_pbxt_h__

#ifdef DRIZZLED
#include <drizzled/common.h>
#include <drizzled/handler.h>
#include <drizzled/plugin/storage_engine.h>
#include <mysys/thr_lock.h>
#else
#include "mysql_priv.h"
#endif

#include "xt_defs.h"
#include "table_xt.h"

#ifdef USE_PRAGMA_INTERFACE
#pragma interface			/* gcc class implementation */
#endif

#if MYSQL_VERSION_ID <= 50120
#define thd_killed(t)		(t)->killed
#endif

#if MYSQL_VERSION_ID >= 50120
#define byte uchar
#endif

class ha_pbxt;

#ifdef DRIZZLED

class PBXTStorageEngine : public StorageEngine {
public:
	PBXTStorageEngine(std::string name_arg)
	: StorageEngine(name_arg, HTON_NO_FLAGS) {}

	/* override */ int close_connection(Session *);
	/* override */ int commit(Session *, bool);
	/* override */ int rollback(Session *, bool);
	/* override */ handler *create(TABLE_SHARE *, MEM_ROOT *);
	/* override */ void drop_database(char *);
	/* override */ bool show_status(Session *, stat_print_fn *, enum ha_stat_type);
};

typedef PBXTStorageEngine handlerton;

#endif

extern handlerton *pbxt_hton;

/*
 * XTShareRec is a structure that will be shared amoung all open handlers.
 */
typedef struct XTShare {
	XTPathStrPtr		sh_table_path;
	uint				sh_use_count;

	XTTableHPtr			sh_table;				/* This is a XTTableHPtr, a reference to the XT internal table handle. */

	uint				sh_dic_key_count;
	XTIndexPtr			*sh_dic_keys;			/* A reference to the XT internal index list. */
	xtBool				sh_recalc_selectivity;	/* This is set to TRUE if when have < 100 rows when the table is openned. */

	/* We use a trick here to get an exclusive lock
	 * on a table. The trick avoids having to use a
	 * semaphore if a thread does not want
	 * exclusive use.
	 */
	xt_mutex_type		*sh_ex_mutex;
	xt_cond_type		*sh_ex_cond;
	xtBool				sh_table_lock;			/* Set to TRUE if a lock on the table is held. */
	ha_pbxt				*sh_handlers;			/* Double linked list of handlers for a particular table. */
	xtWord8				sh_min_auto_inc;		/* Used to proporgate the current auto-inc over a DELETE FROM
												 * (does not work if the server shuts down in between!).
												 */

	THR_LOCK			sh_lock;				/* MySQL lock */
} XTShareRec, *XTSharePtr;

/*
 * Class definition for the storage engine
 */
class ha_pbxt: public handler
{
	public:
	XTSharePtr			pb_share;				/* Shared table info */

	XTOpenTablePtr		pb_open_tab;			/* This is a XTOpenTablePtr (a reference to the XT internal table handle)! */

	xtBool				pb_key_read;			/* No Need to retrieve the entire row, index values are sufficient. */
	int					pb_ignore_dup_key;
	u_int				pb_ind_row_count;

	THR_LOCK_DATA		pb_lock;				/* MySQL lock */

	ha_pbxt				*pb_ex_next;			/* Double linked list of handlers for a particular table. */
	ha_pbxt				*pb_ex_prev;

	xtBool				pb_lock_table;			/* The operation requires a table lock. */
	int					pb_table_locked;		/* TRUE of this handler holds the table lock. */
	int					pb_ex_in_use;			/* Set to 1 while when the handler is in use. */

	THD					*pb_mysql_thd;			/* A pointer to the MySQL thread. */
	xtBool				pb_in_stat;				/* TRUE of start_stmt() was issued */

	ha_pbxt(handlerton *hton, TABLE_SHARE *table_arg);

	virtual ~ha_pbxt() { }

	/* The name that will be used for display purposes */
	const char *table_type() const { return "PBXT"; }

	/*
	 * The name of the index type that will be used for display
	 * don't implement this method unless you really have indexes.
	 */
	const char *index_type(uint inx) { (void) inx; return "BTREE"; }

	const char **bas_ext() const;

	MX_UINT8_T table_cache_type();

	/*
	 * This is a list of flags that says what the storage engine
	 * implements. The current table flags are documented in
	 * handler.h
	 */
	MX_TABLE_TYPES_T table_flags() const;

	/*
	 * part is the key part to check. First key part is 0
	 * If all_parts it's set, MySQL want to know the flags for the combined
	 * index up to and including 'part'.
	 */
	MX_ULONG_T index_flags(uint inx, uint part, bool all_parts) const;

	/*
	 * unireg.cc will call the following to make sure that the storage engine can
	 * handle the data it is about to send.
	 * 
	 * Return *real* limits of your storage engine here. MySQL will do
	 * min(your_limits, MySQL_limits) automatically
	 * 
	 * Theoretically PBXT supports any number of key parts, etc.
	 * Practically this is not true of course.
	 */
	uint	max_supported_record_length()	const { return UINT_MAX; }
	uint	max_supported_keys()			const { return 512; }
	uint	max_supported_key_parts()		const { return 128; }
	uint	max_supported_key_length()		const;
	uint	max_supported_key_part_length() const;

	double	scan_time();

	double	read_time(uint index, uint ranges, ha_rows rows);

  	bool	has_transactions()  { return 1; }

	/*
	 * Everything below are methods that we implement in ha_pbxt.cc.
	 */
	void	internal_close(THD *thd, struct XTThread *self);
	int		open(const char *name, int mode, uint test_if_locked);		// required
	int		reopen(void);
	int		close(void);												// required

	void	init_auto_increment(xtWord8 min_auto_inc);
	void	get_auto_increment(MX_ULONGLONG_T offset, MX_ULONGLONG_T increment,
                                 MX_ULONGLONG_T nb_desired_values,
                                 MX_ULONGLONG_T *first_value,
                                 MX_ULONGLONG_T *nb_reserved_values);
	void	set_auto_increment(Field *nr);

	int		write_row(byte * buf);
	int		update_row(const byte * old_data, byte * new_data);
	int		delete_row(const byte * buf);

	/* Index access functions: */
	int		xt_index_in_range(register XTOpenTablePtr ot, register XTIndexPtr ind, register XTIdxSearchKeyPtr search_key, byte *buf);
	int		xt_index_next_read(register XTOpenTablePtr ot, register XTIndexPtr ind, xtBool key_only, register XTIdxSearchKeyPtr search_key, byte *buf);
	int		xt_index_prev_read(XTOpenTablePtr ot, XTIndexPtr ind, xtBool key_only, register XTIdxSearchKeyPtr search_key, byte *buf);
	int		index_init(uint idx, bool sorted);
	int		index_end();
	int		index_read(byte * buf, const byte * key,
								 uint key_len, enum ha_rkey_function find_flag);
	int		index_read_idx(byte * buf, uint idx, const byte * key,
										 uint key_len, enum ha_rkey_function find_flag);
	int		index_read_xt(byte * buf, uint idx, const byte * key,
										 uint key_len, enum ha_rkey_function find_flag);
	int		index_next(byte * buf);
	int		index_next_same(byte * buf, const byte *key, uint length);
	int		index_prev(byte * buf);
	int		index_first(byte * buf);
	int		index_last(byte * buf);
	int		index_read_last(byte * buf, const byte * key, uint key_len);

	/* Sequential scan functions: */
	int		rnd_init(bool scan);								//required
	int		rnd_end();
	int		rnd_next(byte *buf);								//required
	int		rnd_pos(byte * buf, byte *pos);													 //required
	void	position(const byte *record);			//required
#if MYSQL_VERSION_ID < 50114
	void	info(uint);
#else
	int		info(uint);
#endif

	int		extra(enum ha_extra_function operation);
	int		reset(void);
	int		external_lock(THD *thd, int lock_type);									 //required
	int		start_stmt(THD *thd, thr_lock_type lock_type);
	void	unlock_row();
	int		delete_all_rows(void);
	int		repair(THD* thd, HA_CHECK_OPT* check_opt);
	int		analyze(THD* thd, HA_CHECK_OPT* check_opt);
	int		optimize(THD* thd, HA_CHECK_OPT* check_opt);
	int		check(THD* thd, HA_CHECK_OPT* check_opt);
	ha_rows	records_in_range(uint inx, key_range *min_key, key_range *max_key);
	int		delete_table(const char *from);
	int		delete_system_table(const char *table_path);
	int		rename_table(const char * from, const char * to);
	int		rename_system_table(const char * from, const char * to);
	int		create(const char *name, TABLE *form, HA_CREATE_INFO *create_info);				//required
	void	update_create_info(HA_CREATE_INFO *create_info);

	THR_LOCK_DATA **store_lock(THD *thd, THR_LOCK_DATA **to, enum thr_lock_type lock_type);		 //required

	/* Foreign key support: */
	//bool is_fk_defined_on_table_or_index(uint index);
	char* get_foreign_key_create_info();
	int get_foreign_key_list(THD *thd, List<FOREIGN_KEY_INFO> *f_key_list);
	//bool can_switch_engines();
	uint referenced_by_foreign_key();
	void free_foreign_key_create_info(char* str);

	virtual bool get_error_message(int error, String *buf);
};

/* From ha_pbxt.cc: */
#define XT_TAB_NAME_WITH_EXT_SIZE	XT_TABLE_NAME_SIZE+4

class THD;
struct XTThread;
struct XTDatabase;

void			xt_ha_unlock_table(struct XTThread	*self, void *share);
void			xt_ha_close_global_database(XTThreadPtr self);
void			xt_ha_open_database_of_table(struct XTThread *self, XTPathStrPtr table_path);
struct XTThread	*xt_ha_set_current_thread(THD *thd, XTExceptionPtr e);
void			xt_ha_close_connection(THD* thd);
struct XTThread	*xt_ha_thd_to_self(THD* thd);
int				xt_ha_pbxt_to_mysql_error(int xt_err);
int				xt_ha_pbxt_thread_error_for_mysql(THD *thd, const XTThreadPtr self, int ignore_dup_key);
void			xt_ha_all_threads_close_database(XTThreadPtr self, XTDatabase *db);

/*
 * These hooks are suppossed to only be used by InnoDB:
 */
#ifndef DRIZZLED
#ifdef INNODB_COMPATIBILITY_HOOKS
extern "C" struct charset_info_st *thd_charset(MYSQL_THD thd);
extern "C" char **thd_query(MYSQL_THD thd);
extern "C" int thd_slave_thread(const MYSQL_THD thd);
extern "C" int thd_non_transactional_update(const MYSQL_THD thd);
extern "C" int thd_binlog_format(const MYSQL_THD thd);
extern "C" void thd_mark_transaction_to_rollback(MYSQL_THD thd, bool all);
#else
#define thd_charset(t)						(t)->charset()
#define thd_query(t)						&(t)->query
#define thd_slave_thread(t)					(t)->slave_thread
#define thd_non_transactional_update(t)		(t)->transaction.all.modified_non_trans_table
#define thd_binlog_format(t)				(t)->variables.binlog_format
#define thd_mark_transaction_to_rollback(t)	mark_transaction_to_rollback(t, all)
#endif // INNODB_COMPATIBILITY_HOOKS */
#endif /* !DRIZZLED */

/* How to lock MySQL mutexes! */
#ifdef SAFE_MUTEX

#if MYSQL_VERSION_ID < 60000
#if MYSQL_VERSION_ID < 50123
#define myxt_mutex_lock(x)		safe_mutex_lock(x,__FILE__,__LINE__)
#else
#define myxt_mutex_lock(x)		safe_mutex_lock(x,0,__FILE__,__LINE__)
#endif
#else
#if MYSQL_VERSION_ID < 60004
#define myxt_mutex_lock(x)		safe_mutex_lock(x,__FILE__,__LINE__)
#else
#define myxt_mutex_lock(x)		safe_mutex_lock(x,0,__FILE__,__LINE__)
#endif
#endif

#define myxt_mutex_t			safe_mutex_t
#define myxt_mutex_unlock(x)	safe_mutex_unlock(x,__FILE__,__LINE__)

#else // SAFE_MUTEX

#ifdef MY_PTHREAD_FASTMUTEX
#define myxt_mutex_lock(x)		my_pthread_fastmutex_lock(x)
#define myxt_mutex_t			my_pthread_fastmutex_t
#define myxt_mutex_unlock(x)	pthread_mutex_unlock(&(x)->mutex)
#else
#define myxt_mutex_lock(x)		pthread_mutex_lock(x)
#define myxt_mutex_t			pthread_mutex_t
#define myxt_mutex_unlock(x)	pthread_mutex_unlock(x)
#endif

#endif // SAFE_MUTEX

#endif

