/* Copyright (c) 2009 PrimeBase Technologies GmbH
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
 * 2009-09-07	Paul McCullagh
 *
 * H&G2JCtL
 */

#include "xt_config.h"

#ifdef MYSQL_SUPPORTS_BACKUP

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <ctype.h>

#include "mysql_priv.h"
#include <backup/api_types.h>
#include <backup/backup_engine.h>
#include <backup/backup_aux.h>         // for build_table_list()
#include <hash.h>

#include "ha_pbxt.h"

#include "backup_xt.h"
#include "pthread_xt.h"
#include "filesys_xt.h"
#include "database_xt.h"
#include "strutil_xt.h"
#include "memory_xt.h"
#include "trace_xt.h"
#include "myxt_xt.h"

#ifdef OK
#undef OK
#endif

#ifdef byte
#undef byte
#endif

#ifdef DEBUG
//#define TRACE_BACKUP_CALLS
//#define TEST_SMALL_BLOCK			100000
#endif

using backup::byte;
using backup::result_t;
using backup::version_t;
using backup::Table_list;
using backup::Table_ref;
using backup::Buffer;

#ifdef TRACE_BACKUP_CALLS
#define XT_TRACE_CALL()				ha_trace_function(__FUNC__, NULL)
#else
#define XT_TRACE_CALL()
#endif

#define XT_RESTORE_BATCH_SIZE		10000

#define BUP_STATE_BEFORE_LOCK		0
#define BUP_STATE_AFTER_LOCK		1

#define BUP_STANDARD_VAR_RECORD		1
#define BUP_RECORD_BLOCK_4_START	2			// Part of a record, with a 4 byte total length, and 4 byte data length
#define BUP_RECORD_BLOCK_4			3			// Part of a record, with a 4 byte length
#define BUP_RECORD_BLOCK_4_END		4			// Last part of a record with a 4 byte length

/*
 * -----------------------------------------------------------------------
 * UTILITIES
 */

#ifdef TRACE_BACKUP_CALLS
static void ha_trace_function(const char *function, char *table)
{
	char		func_buf[50], *ptr;
	XTThreadPtr	thread = xt_get_self(); 
	
	if ((ptr = strchr(function, '('))) {
		ptr--;
		while (ptr > function) {
			if (!(isalnum(*ptr) || *ptr == '_'))
				break;
			ptr--;
		}
		ptr++;
		xt_strcpy(50, func_buf, ptr);
		if ((ptr = strchr(func_buf, '(')))
			*ptr = 0;
	}
	else
		xt_strcpy(50, func_buf, function);
	if (table)
		printf("%s %s (%s)\n", thread ? thread->t_name : "-unknown-", func_buf, table);
	else
		printf("%s %s\n", thread ? thread->t_name : "-unknown-", func_buf);
}
#endif

/*
 * -----------------------------------------------------------------------
 * BACKUP DRIVER
 */

class PBXTBackupDriver: public Backup_driver
{
	public:
	PBXTBackupDriver(const Table_list &);
	virtual ~PBXTBackupDriver();

	virtual size_t		size();
	virtual size_t		init_size();
	virtual result_t	begin(const size_t);
	virtual result_t	end();
	virtual result_t	get_data(Buffer &);
	virtual result_t	prelock();
	virtual result_t	lock();
	virtual result_t	unlock();
	virtual result_t	cancel();
	virtual void		free();
	void				lock_tables_TL_READ_NO_INSERT();

	private:
	XTThreadPtr		bd_thread;
	int				bd_state;
	u_int			bd_table_no;
	XTOpenTablePtr	bd_ot;
	xtWord1			*bd_row_buf;

	/* Non-zero if we last returned only part of
	 * a row.
	 */
	xtWord1			*db_write_block(xtWord1 *buffer, xtWord1 bup_type, size_t *size, xtWord4 row_len);
	xtWord1			*db_write_block(xtWord1 *buffer, xtWord1 bup_type, size_t *size, xtWord4 total_len, xtWord4 row_len);

	xtWord4			bd_row_offset;
	xtWord4			bd_row_size;
};


PBXTBackupDriver::PBXTBackupDriver(const Table_list &tables):
Backup_driver(tables),
bd_state(BUP_STATE_BEFORE_LOCK),
bd_table_no(0),
bd_ot(NULL),
bd_row_buf(NULL),
bd_row_offset(0),
bd_row_size(0)
{
}

PBXTBackupDriver::~PBXTBackupDriver()
{
}

/** Estimates total size of backup. @todo improve it */
size_t PBXTBackupDriver::size()
{
	XT_TRACE_CALL();
	return UNKNOWN_SIZE;
}

/** Estimates size of backup before lock. @todo improve it */
size_t PBXTBackupDriver::init_size()
{
	XT_TRACE_CALL();
	return 0;
}

result_t PBXTBackupDriver::begin(const size_t)
{
	THD				*thd = current_thd;
	XTExceptionRec	e;

	XT_TRACE_CALL();
	
	if (!(bd_thread = xt_ha_set_current_thread(thd, &e))) {
		xt_log_exception(NULL, &e, XT_LOG_DEFAULT);
		return backup::ERROR;
	}
	
	return backup::OK;
}

result_t PBXTBackupDriver::end()
{
	XT_TRACE_CALL();
	if (bd_ot) {
		xt_tab_seq_exit(bd_ot);
		xt_db_return_table_to_pool_ns(bd_ot);
		bd_ot = NULL;
	}
	if (bd_thread->st_xact_data) {
		if (!xt_xn_commit(bd_thread))
			return backup::ERROR;
	}
	return backup::OK;
}

xtWord1 *PBXTBackupDriver::db_write_block(xtWord1 *buffer, xtWord1 bup_type, size_t *ret_size, xtWord4 row_len)
{
	register size_t size = *ret_size;

	*buffer = bup_type;	// Record type identifier.
	buffer++;
	size--;
	memcpy(buffer, bd_ot->ot_row_wbuffer, row_len);
	buffer += row_len;
	size -= row_len;
	*ret_size = size;
	return buffer;
}

xtWord1 *PBXTBackupDriver::db_write_block(xtWord1 *buffer, xtWord1 bup_type, size_t *ret_size, xtWord4 total_len, xtWord4 row_len)
{
	register size_t size = *ret_size;

	*buffer = bup_type;	// Record type identifier.
	buffer++;
	size--;
	if (bup_type == BUP_RECORD_BLOCK_4_START) {
		XT_SET_DISK_4(buffer, total_len);
		buffer += 4;
		size -= 4;
	}
	XT_SET_DISK_4(buffer, row_len);
	buffer += 4;
	size -= 4;
	memcpy(buffer, bd_ot->ot_row_wbuffer+bd_row_offset, row_len);
	buffer += row_len;
	size -= row_len;
	bd_row_size -= row_len;
	bd_row_offset += row_len;
	*ret_size = size;
	return buffer;
}

result_t PBXTBackupDriver::get_data(Buffer &buf)
{
	xtBool	eof = FALSE;
	size_t	size;
	xtWord4	row_len;
	xtWord1	*buffer;

	XT_TRACE_CALL();

	if (bd_state == BUP_STATE_BEFORE_LOCK) {
		buf.table_num = 0;
		buf.size = 0;
		buf.last = FALSE;
		return backup::READY;
	}

	/* Open the backup table: */
	if (!bd_ot) {
		XTThreadPtr		self = bd_thread;
		XTTableHPtr		tab;
		char			path[PATH_MAX];
	
		if (bd_table_no == m_tables.count()) {
			buf.size = 0;
			buf.table_num = 0;
			buf.last = TRUE;
			return backup::DONE;
		}
		
		m_tables[bd_table_no].internal_name(path, sizeof(path));
		bd_table_no++;
		try_(a)	{
			xt_ha_open_database_of_table(self, (XTPathStrPtr) path);
			tab = xt_use_table(self, (XTPathStrPtr) path, FALSE, FALSE);
			pushr_(xt_heap_release, tab);
			if (!(bd_ot = xt_db_open_table_using_tab(tab, bd_thread)))
				xt_throw(self);
			freer_(); // xt_heap_release(tab)

			/* Prepare the seqential scan: */
			xt_tab_seq_exit(bd_ot);
			if (!xt_tab_seq_init(bd_ot))
				xt_throw(self);
			
			if (bd_row_buf) {
				xt_free(self, bd_row_buf);
				bd_row_buf = NULL;
			}
			bd_row_buf = (xtWord1 *) xt_malloc(self, bd_ot->ot_table->tab_dic.dic_mysql_buf_size);
			bd_ot->ot_cols_req = bd_ot->ot_table->tab_dic.dic_no_of_cols;
		}
		catch_(a) {
			;
		}
		cont_(a);

		if (!bd_ot)
			goto failed;
	}

	buf.table_num = bd_table_no;
#ifdef TEST_SMALL_BLOCK
	buf.size = TEST_SMALL_BLOCK;
#endif
	size = buf.size;
	buffer = (xtWord1 *) buf.data;
	ASSERT_NS(size > 9);

	/* First check of a record was partically written
	 * last time.
	 */
	write_row:
	if (bd_row_size > 0) {
		row_len = bd_row_size;
		if (bd_row_offset == 0) {
			if (row_len+1 > size) {
				ASSERT_NS(size > 9);
				row_len = size - 9;
				buffer = db_write_block(buffer, BUP_RECORD_BLOCK_4_START, &size, bd_row_size, row_len);
				goto done;
			}
			buffer = db_write_block(buffer, BUP_STANDARD_VAR_RECORD, &size, row_len);
			bd_row_size = 0;
		}
		else {
			if (row_len+5 > size) {
				row_len = size - 5;
				buffer = db_write_block(buffer, BUP_RECORD_BLOCK_4, &size, 0, row_len);
				goto done;
			}
			buffer = db_write_block(buffer, BUP_RECORD_BLOCK_4_END, &size, 0, row_len);
		}
	}

	/* Now continue with the sequential scan. */
	while (size > 1) {
		if (!xt_tab_seq_next(bd_ot, bd_row_buf, &eof))
			goto failed;
		if (eof) {
			/* We will go the next table, on the next call. */
			xt_tab_seq_exit(bd_ot);
			xt_db_return_table_to_pool_ns(bd_ot);
			bd_ot = NULL;
			break;
		}
		if (!(row_len = myxt_store_row_data(bd_ot, 0, (char *) bd_row_buf)))
			goto failed;
		if (row_len+1 > size) {
			/* Does not fit: */
			bd_row_offset = 0;
			bd_row_size = row_len;
			/* Only add part of the row, if there is still
			 * quite a bit of space left:
			 */
			if (size >= (32 * 1024))
				goto write_row;
			break;
		}
		buffer = db_write_block(buffer, BUP_STANDARD_VAR_RECORD, &size, row_len);
	}

	done:
	buf.size = buf.size - size;
	/* This indicates wnd of data for a table! */
    buf.last = eof;

	return backup::OK;

	failed:
	xt_log_and_clear_exception(bd_thread);
	return backup::ERROR;
}

result_t PBXTBackupDriver::prelock()
{
	XT_TRACE_CALL();
	return backup::READY;
}

result_t PBXTBackupDriver::lock()
{
	XT_TRACE_CALL();
	bd_thread->st_xact_mode = XT_XACT_COMMITTED_READ;
	bd_thread->st_ignore_fkeys = FALSE;
	bd_thread->st_auto_commit = FALSE;
	bd_thread->st_table_trans = FALSE;
	bd_thread->st_abort_trans = FALSE;
	bd_thread->st_stat_ended = FALSE;
	bd_thread->st_stat_trans = FALSE;
	bd_thread->st_is_update = NULL;
	if (!xt_xn_begin(bd_thread))
		return backup::ERROR;
	bd_state = BUP_STATE_AFTER_LOCK;
	return backup::OK;
}

result_t PBXTBackupDriver::unlock()
{
	XT_TRACE_CALL();
	return backup::OK;
}

result_t PBXTBackupDriver::cancel()
{
	XT_TRACE_CALL();
	return backup::OK; // free() will be called and suffice
}

void PBXTBackupDriver::free()
{
	XT_TRACE_CALL();
	if (bd_ot) {
		xt_tab_seq_exit(bd_ot);
		xt_db_return_table_to_pool_ns(bd_ot);
		bd_ot = NULL;
	}
	if (bd_row_buf) {
		xt_free_ns(bd_row_buf);
		bd_row_buf = NULL;
	}
	if (bd_thread->st_xact_data)
		xt_xn_rollback(bd_thread);
	delete this;
}

void PBXTBackupDriver::lock_tables_TL_READ_NO_INSERT()
{
	XT_TRACE_CALL();
}

/*
 * -----------------------------------------------------------------------
 * BACKUP DRIVER
 */

class PBXTRestoreDriver: public Restore_driver
{
	public:
	PBXTRestoreDriver(const Table_list &tables);
	virtual ~PBXTRestoreDriver();

	virtual result_t  begin(const size_t);
	virtual result_t  end();
	virtual result_t  send_data(Buffer &buf);
	virtual result_t  cancel();
	virtual void      free();
	
	private:
	XTThreadPtr		rd_thread;
	u_int			rd_table_no;
	XTOpenTablePtr	rd_ot;
	STRUCT_TABLE	*rd_my_table;
	xtWord1			*rb_row_buf;
	u_int			rb_col_cnt;
	u_int			rb_insert_count;

	/* Long rows are accumulated here: */
	xtWord4			rb_row_len;
	xtWord4			rb_data_size;
	xtWord1			*rb_row_data;
};

PBXTRestoreDriver::PBXTRestoreDriver(const Table_list &tables):
Restore_driver(tables),
rd_thread(NULL),
rd_table_no(0),
rd_ot(NULL),
rb_row_buf(NULL),
rb_row_len(0),
rb_data_size(0),
rb_row_data(NULL)
{
}

PBXTRestoreDriver::~PBXTRestoreDriver()
{
}

result_t PBXTRestoreDriver::begin(const size_t)
{
	THD				*thd = current_thd;
	XTExceptionRec	e;
	
	XT_TRACE_CALL();
	
	if (!(rd_thread = xt_ha_set_current_thread(thd, &e))) {
		xt_log_exception(NULL, &e, XT_LOG_DEFAULT);
		return backup::ERROR;
	}
	
	return backup::OK;
}

result_t PBXTRestoreDriver::end()
{
	XT_TRACE_CALL();
	if (rd_ot) {
		xt_db_return_table_to_pool_ns(rd_ot);
		rd_ot = NULL;
	}
	//if (rb_row_buf) {
	//	xt_free_ns(rb_row_buf);
	//	rb_row_buf = NULL;
	//}
	if (rb_row_data) {
		xt_free_ns(rb_row_data);
		rb_row_data = NULL;
	}
	if (rd_thread->st_xact_data) {
		if (!xt_xn_commit(rd_thread))
			return backup::ERROR;
	}
	return backup::OK;
}


result_t PBXTRestoreDriver::send_data(Buffer &buf)
{
	size_t	size;
	xtWord1	type;
	xtWord1	*buffer;
	xtWord4	row_len;
	xtWord1 *rec_data;

	XT_TRACE_CALL();

	if (buf.table_num != rd_table_no) {
		XTThreadPtr		self = rd_thread;
		XTTableHPtr		tab;
		char			path[PATH_MAX];
		
		if (rd_ot) {
			xt_db_return_table_to_pool_ns(rd_ot);
			rd_ot = NULL;
		}

		if (rd_thread->st_xact_data) {
			if (!xt_xn_commit(rd_thread))
				goto failed;
		}
		if (!xt_xn_begin(rd_thread))
			goto failed;
		rb_insert_count = 0;
		
		rd_table_no = buf.table_num;
		m_tables[rd_table_no-1].internal_name(path, sizeof(path));
		try_(a)	{
			xt_ha_open_database_of_table(self, (XTPathStrPtr) path);
			tab = xt_use_table(self, (XTPathStrPtr) path, FALSE, FALSE);
			pushr_(xt_heap_release, tab);
			if (!(rd_ot = xt_db_open_table_using_tab(tab, rd_thread)))
				xt_throw(self);
			freer_(); // xt_heap_release(tab)

			rd_my_table = rd_ot->ot_table->tab_dic.dic_my_table;
			if (rd_my_table->found_next_number_field) {
				rd_my_table->in_use = current_thd;
				rd_my_table->next_number_field = rd_my_table->found_next_number_field;
				rd_my_table->mark_columns_used_by_index_no_reset(rd_my_table->s->next_number_index, rd_my_table->read_set);
			}

			/* This is safe because only one thread can restore a table at 
			 * a time!
			 */
			rb_row_buf = (xtWord1 *) rd_my_table->record[0];
			//if (rb_row_buf) {
			//	xt_free(self, rb_row_buf);
			//	rb_row_buf = NULL;
			//}
			//rb_row_buf = (xtWord1 *) xt_malloc(self, rd_ot->ot_table->tab_dic.dic_mysql_buf_size);
	
			rb_col_cnt = rd_ot->ot_table->tab_dic.dic_no_of_cols;

		}
		catch_(a) {
			;
		}
		cont_(a);
		
		if (!rd_ot)
			goto failed;
	}

	buffer = (xtWord1 *) buf.data;
	size = buf.size;

	while (size > 0) {
		type = *buffer;
		switch (type) {
			case BUP_STANDARD_VAR_RECORD:
				rec_data = buffer + 1;
				break;
			case BUP_RECORD_BLOCK_4_START:
				buffer++;
				row_len = XT_GET_DISK_4(buffer);
				buffer += 4;
				if (rb_data_size < row_len) {
					if (!xt_realloc_ns((void **) &rb_row_data, row_len))
						goto failed;
					rb_data_size = row_len;
				}
				row_len = XT_GET_DISK_4(buffer);
				buffer += 4;
				ASSERT_NS(row_len <= rb_data_size);
				if (row_len > rb_data_size) {
					xt_register_xterr(XT_REG_CONTEXT, XT_ERR_BAD_BACKUP_FORMAT);
					goto failed;
				}
				memcpy(rb_row_data, buffer, row_len);
				rb_row_len = row_len;
				buffer += row_len;
				if (row_len + 9 > size) {
					xt_register_xterr(XT_REG_CONTEXT, XT_ERR_BAD_BACKUP_FORMAT);
					goto failed;
				}
				size -= row_len + 9;
				continue;
			case BUP_RECORD_BLOCK_4:
				buffer++;
				row_len = XT_GET_DISK_4(buffer);
				buffer += 4;
				ASSERT_NS(rb_row_len + row_len <= rb_data_size);
				if (rb_row_len + row_len > rb_data_size) {
					xt_register_xterr(XT_REG_CONTEXT, XT_ERR_BAD_BACKUP_FORMAT);
					goto failed;
				}
				memcpy(rb_row_data + rb_row_len, buffer, row_len);
				rb_row_len += row_len;
				buffer += row_len;
				if (row_len + 5 > size) {
					xt_register_xterr(XT_REG_CONTEXT, XT_ERR_BAD_BACKUP_FORMAT);
					goto failed;
				}
				size -= row_len + 5;
				continue;
			case BUP_RECORD_BLOCK_4_END:
				buffer++;
				row_len = XT_GET_DISK_4(buffer);
				buffer += 4;
				ASSERT_NS(rb_row_len + row_len <= rb_data_size);
				if (rb_row_len + row_len > rb_data_size) {
					xt_register_xterr(XT_REG_CONTEXT, XT_ERR_BAD_BACKUP_FORMAT);
					goto failed;
				}
				memcpy(rb_row_data + rb_row_len, buffer, row_len);
				buffer += row_len;
				if (row_len + 5 > size) {
					xt_register_xterr(XT_REG_CONTEXT, XT_ERR_BAD_BACKUP_FORMAT);
					goto failed;
				}
				size -= row_len + 5;
				rec_data = rb_row_data;
				break;
			default:
				xt_register_xterr(XT_REG_CONTEXT, XT_ERR_BAD_BACKUP_FORMAT);
				goto failed;
		}
		
		if (!(row_len = myxt_load_row_data(rd_ot, rec_data, rb_row_buf, rb_col_cnt)))
			goto failed;

		if (rd_ot->ot_table->tab_dic.dic_my_table->found_next_number_field)
			ha_set_auto_increment(rd_ot, rd_ot->ot_table->tab_dic.dic_my_table->found_next_number_field);

		if (!xt_tab_new_record(rd_ot, rb_row_buf))
			goto failed;

		if (type == BUP_STANDARD_VAR_RECORD) {
			buffer += row_len+1;
			if (row_len + 1 > size) {
				xt_register_xterr(XT_REG_CONTEXT, XT_ERR_BAD_BACKUP_FORMAT);
				goto failed;
			}
			size -= row_len + 1;
		}

		rb_insert_count++;
		if (rb_insert_count == XT_RESTORE_BATCH_SIZE) {
			if (!xt_xn_commit(rd_thread))
				goto failed;
			if (!xt_xn_begin(rd_thread))
				goto failed;
			rb_insert_count = 0;
		}
	}

	return backup::OK;
	
	failed:
	xt_log_and_clear_exception(rd_thread);
	return backup::ERROR;
}


result_t PBXTRestoreDriver::cancel()
{
	XT_TRACE_CALL();
	/* Nothing to do in cancel(); free() will suffice */
	return backup::OK;
}

void PBXTRestoreDriver::free()
{
	XT_TRACE_CALL();
	if (rd_ot) {
		xt_db_return_table_to_pool_ns(rd_ot);
		rd_ot = NULL;
	}
	//if (rb_row_buf) {
	//	xt_free_ns(rb_row_buf);
	//	rb_row_buf = NULL;
	//}
	if (rb_row_data) {
		xt_free_ns(rb_row_data);
		rb_row_data = NULL;
	}
	if (rd_thread->st_xact_data)
		xt_xn_rollback(rd_thread);
	delete this;
}

/*
 * -----------------------------------------------------------------------
 * BACKUP ENGINE FACTORY
 */

#define PBXT_BACKUP_VERSION 1


class PBXTBackupEngine: public Backup_engine
{
	public:
	PBXTBackupEngine() { };

	virtual version_t version() const {
		return PBXT_BACKUP_VERSION;
	};

	virtual result_t get_backup(const uint32, const Table_list &, Backup_driver* &);

	virtual result_t get_restore(const version_t, const uint32, const Table_list &,Restore_driver* &);

	virtual void free()
	{
		delete this;
	}
};

result_t PBXTBackupEngine::get_backup(const u_int count, const Table_list &tables, Backup_driver* &drv)
{
	PBXTBackupDriver *ptr = new PBXTBackupDriver(tables);

	if (!ptr)
		return backup::ERROR;
	drv = ptr;
	return backup::OK;
}

result_t PBXTBackupEngine::get_restore(const version_t ver, const uint32,
                             const Table_list &tables, Restore_driver* &drv)
{
	if (ver > PBXT_BACKUP_VERSION)
	{
		return backup::ERROR;    
	}
	
	PBXTRestoreDriver *ptr = new PBXTRestoreDriver(tables);

	if (!ptr)
		return backup::ERROR;
	drv = (Restore_driver *) ptr;
	return backup::OK;
}


Backup_result_t pbxt_backup_engine(handlerton *self, Backup_engine* &be)
{
	be = new PBXTBackupEngine();
	
	if (!be)
		return backup::ERROR;
	
	return backup::OK;
}

#endif
