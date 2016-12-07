/*****************************************************************************

Copyright (c) 2016, Oracle and/or its affiliates. All Rights Reserved.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA

*****************************************************************************/

/** @file dict/dict0dd.cc
Data dictionary interface */

#include <current_thd.h>

#include "dict0dd.h"
#include "dict0dict.h"
#include "dict0crea.h"
#include "dict0priv.h"
#include <dd/properties.h>
#include "dict0mem.h"
#include "dict0stats.h"
#include "rem0rec.h"
#include "data0type.h"
#include "mach0data.h"
#include "dict0dict.h"
#include "fts0priv.h"
#include "ut0crc32.h"
#include "srv0start.h"
#include "sql_table.h"
#include "sql_base.h"

/** Acquire a metadata lock.
@param[in,out]	thd	current thread
@param[out]	mdl	metadata lock
@param[in]	db	schema name
@param[in]	table	table name
@retval	false if acquired
@retval	true if failed (my_error() will have been called) */
static
bool
dd_mdl_acquire(
	THD*			thd,
	MDL_ticket**		mdl,
	const char*		db,
	char*			table)
{
	bool	ret = false;

	/* If InnoDB acquires MDL lock on partition table, it always
	acquires on its parent table name */
	char*   is_part = NULL;
#ifdef _WIN32
                is_part = strstr(table, "?p?");
#else
                is_part = strstr(table, "?P?");
#endif /* _WIN32 */

	if (is_part) {
		*is_part ='\0';
	}

	ret = dd::acquire_shared_table_mdl(thd, db, table, false, mdl);

	if (is_part) {
		*is_part = '?';
	}

	return(ret);
}

#ifdef UNIV_DEBUG

/** Verify a metadata lock.
@param[in,out]	thd	current thread
@param[in]	db	schema name
@param[in]	table	table name
@retval	false if acquired
@retval	true if lock is there */
static
bool
dd_mdl_verify(
	THD*			thd,
	const char*		db,
	char*			table)
{
	bool	ret = false;

	/* If InnoDB acquires MDL lock on partition table, it always
	acquires on its parent table name */
	char*   is_part = NULL;
#ifdef _WIN32
                is_part = strstr(table, "?p?");
#else
                is_part = strstr(table, "?P?");
#endif /* _WIN32 */

	if (is_part) {
		*is_part ='\0';
	}

	ret = dd::has_shared_table_mdl(thd, db, table);

	if (is_part) {
		*is_part = '?';
	}

	return(ret);
}

#endif /* UNIV_DEBUG */

/** Release a metadata lock.
@param[in,out]	thd	current thread
@param[in,out]	mdl	metadata lock */
void
dd_mdl_release(
	THD*		thd,
	MDL_ticket**	mdl)
{
	ut_ad(*mdl != nullptr);
	dd::release_mdl(thd, *mdl);
	*mdl = nullptr;
}

/** Instantiate an InnoDB in-memory table metadata (dict_table_t)
based on a Global DD object.
@param[in,out]	client		data dictionary client
@param[in]	dd_table	Global DD table object
@param[in]	dd_part		Global DD partition or subpartition, or NULL
@param[in]	tbl_name	table name, or NULL if not known
@param[in,out]	uncached	NULL if the table should be added to the cache;
				if not, *uncached=true will be assigned
				when ib_table was allocated but not cached
				(used during delete_table and rename_table)
@param[out]	table		InnoDB table (NULL if not found or loadable)
@param[in]	skip_mdl	whether meta-data locking is skipped
@return	error code
@retval	0	on success */
int
dd_table_open_on_dd_obj(
	dd::cache::Dictionary_client*	client,
	const dd::Table&		dd_table,
	const dd::Partition*		dd_part,
	const char*			tbl_name,
	bool*				uncached,
	dict_table_t*&			table,
	bool				skip_mdl,
	THD*				thd)
{
	ut_ad(dd_part == nullptr || &dd_part->table() == &dd_table);
	ut_ad(dd_part == nullptr
	      || dd_table.se_private_id() == dd::INVALID_OBJECT_ID);
	ut_ad(dd_part == nullptr
	      || dd_table.partition_type() != dd::Table::PT_NONE);
	ut_ad(dd_part == nullptr
	      || dd_part->level() == (dd_part->parent() != nullptr));
	ut_ad(dd_part == nullptr
	      || ((dd_part->table().subpartition_type() != dd::Table::ST_NONE)
		  == (dd_part->parent() != nullptr)));
	ut_ad(dd_part == nullptr
	      || dd_part->parent() == nullptr
	      || dd_part->parent()->level() == 0);
#ifdef UNIV_DEBUG
	if (tbl_name) {
		char	db_buf[NAME_LEN + 1];
		char	tbl_buf[NAME_LEN + 1];

		innobase_parse_tbl_name(tbl_name, db_buf, tbl_buf);
		if (dd_part == NULL) {
			ut_ad(strcmp(dd_table.name().c_str(), tbl_buf) == 0);
		} else {
			ut_ad(strncmp(dd_table.name().c_str(), tbl_buf,
				      dd_table.name().size()) == 0);
		}
		ut_ad(skip_mdl
		      || dd_mdl_verify(thd, db_buf, tbl_buf));
	}
#endif /* UNIV_DEBUG */

	int			error		= 0;
	const uint64		table_id	= dd_part == nullptr
		? dd_table.se_private_id()
		: dd_part->se_private_id();
	const ulint		fold		= ut_fold_ull(table_id);
#ifdef UNIV_DEBUG
	const bool		is_temp
		= (table_id > dict_sys_t::NUM_HARD_CODED_TABLES)
		&& !dd_table.is_persistent();
#endif

	ut_ad(table_id != dd::INVALID_OBJECT_ID);

	mutex_enter(&dict_sys->mutex);

	HASH_SEARCH(id_hash, dict_sys->table_id_hash, fold,
                    dict_table_t*, table, ut_ad(table->cached),
                    table->id == table_id);


	if (table == nullptr) {
		ut_ad(!is_temp);
	} else {
		if (uncached == nullptr) {
			ut_ad(!table->is_corrupted());
		}

		if (table != nullptr) {
			table->acquire();
		}
	}

	mutex_exit(&dict_sys->mutex);

#if 0
	ut_ad(table == nullptr
	      || (dd_part == nullptr
		  ? dd_table_check(dd_table, *table, true)
		  : dd_table_check(*dd_part, *table, true)));
#endif

	if (table || error) {
		return(error);
	}

	TABLE_SHARE		ts;
	dd::Schema*		schema;
	const char*		table_cache_key;
	size_t			table_cache_key_len;

	if (tbl_name != nullptr) {
		schema = nullptr;
		table_cache_key = tbl_name;
		table_cache_key_len = dict_get_db_name_len(tbl_name);
	} else {
		error = client->acquire_uncached<dd::Schema>(
			dd_table.schema_id(), &schema);

		if (error) {
			return(error);
		}

		table_cache_key = schema->name().c_str();
		table_cache_key_len = schema->name().size();
	}

	init_tmp_table_share(thd,
			     &ts, table_cache_key, table_cache_key_len,
			     dd_table.name().c_str(), ""/* file name */);

	error = open_table_def(thd, &ts, false, &dd_table);

	if (!error) {
		TABLE	td;

		error = open_table_from_share(thd, &ts,
					      dd_table.name().c_str(),
					      0, OPEN_FRM_FILE_ONLY, 0,
					      &td, false, &dd_table);
		if (!error) {
#if 0
			error = dd_part == nullptr
				? dd_convert_table(
					client, &td, nullptr, uncached, table,
					dd_table, skip_mdl)
				: dd_convert_part(
					client, &td, uncached, table,
					*dd_part, skip_mdl);
#endif
			char		tmp_name[2 * (NAME_LEN + 1)];
			const char*	tab_namep;

			if (tbl_name) {
				tab_namep = tbl_name;
			} else {
				snprintf(tmp_name, sizeof tmp_name,
					 "%s/%s", schema->name().c_str(),
					 dd_table.name().c_str());
				tab_namep = tmp_name;
			}

			table = dd_open_table(
				client, &td, tab_namep, uncached, table,
				&dd_table, skip_mdl, thd);
		}

		closefrm(&td, false);
	}

	free_table_share(&ts);

	return(error);
}

/** Load an InnoDB table definition by InnoDB table ID.
@param[in,out]	thd		current thread
@param[in,out]	mdl		metadata lock;
nullptr if we are resurrecting table IX locks in recovery
@param[in]	tbl_name	table name for already granted MDL,
or nullptr if mdl==nullptr or *mdl==nullptr
@param[in]	table_id	InnoDB table or partition ID
@return	InnoDB table
@retval	nullptr	if the table is not found, or there was an error */
static
dict_table_t*
dd_table_open_on_id_low(
	THD*			thd,
	MDL_ticket**		mdl,
	const char*		tbl_name,
	table_id_t		table_id)
{
	ut_ad(thd == nullptr || thd == current_thd);
#ifdef UNIV_DEBUG
	btrsea_sync_check	check(false);
	ut_ad(!sync_check_iterate(check));
#endif
	ut_ad(!srv_is_being_shutdown);

	if (thd == nullptr) {
		ut_ad(mdl == nullptr);
		thd = current_thd;
	}

#ifdef UNIV_DEBUG
	char	db_buf[NAME_LEN + 1];
	char	tbl_buf[NAME_LEN + 1];

	if (tbl_name) {
		innobase_parse_tbl_name(tbl_name, db_buf, tbl_buf);
		ut_ad(dd_mdl_verify(thd, db_buf, tbl_buf));
	}
#endif /* UNIV_DEBUG */

	const dd::Table*				dd_table;
	const dd::Partition*				dd_part	= nullptr;
	dd::cache::Dictionary_client*			dc
		= dd::get_dd_client(thd);
	dd::cache::Dictionary_client::Auto_releaser	releaser(dc);

	for (;;) {
		dd::String_type	schema;
		dd::String_type	tablename;
		if (dc->get_table_name_by_se_private_id(handler_name,
							table_id,
							&schema, &tablename)) {
			return(nullptr);
		}

		const bool	not_table = schema.empty();

		if (not_table) {
			if (dc->get_table_name_by_partition_se_private_id(
				    handler_name, table_id,
				    &schema, &tablename)
			    || schema.empty()) {
				return(nullptr);
			}
		}

		if (mdl != nullptr) {
			if (*mdl == reinterpret_cast<MDL_ticket*>(-1)) {
				*mdl = nullptr;
			}

			ut_ad((*mdl == nullptr) == (!tbl_name));
#ifdef UNIV_DEBUG
			if (*mdl != nullptr) {

				ut_ad(strcmp(schema.c_str(), db_buf) == 0);
				if (not_table) {
					ut_ad(strncmp(tablename.c_str(),
						      tbl_buf,
						      strlen(tablename.c_str()))
					      == 0);
				} else {
					ut_ad(strcmp(tablename.c_str(),
						     tbl_buf) == 0);
				}
			}
#endif

			if (*mdl == nullptr && dd_mdl_acquire(
				    thd, mdl,
				    schema.c_str(),
				    const_cast<char*>(tablename.c_str()))) {
				return(nullptr);
			}

			ut_ad(*mdl != nullptr);
		}

		if (dc->acquire(schema, tablename, &dd_table)
		    || dd_table == nullptr) {
			if (mdl != nullptr) {
				dd_mdl_release(thd, mdl);
			}
			return(nullptr);
		}

		const bool	is_part
			= (dd_table->partition_type() != dd::Table::PT_NONE);
		bool		same_name = not_table == is_part
			&& (not_table || dd_table->se_private_id() == table_id)
			&& dd_table->engine() == handler_name;

		if (same_name && is_part) {
			auto end = dd_table->partitions().end();
			auto i = std::search_n(
				dd_table->partitions().begin(), end, 1,
				table_id,
				[](const dd::Partition* p, table_id_t id)
				{
					return(p->se_private_id() == id);
				});

			if (i == end) {
				same_name = false;
			} else {
				dd_part = *i;
				/* TODO: NEW_DD, unquote after partition
				WL#9162 is ported
				ut_ad(dd_part_is_stored(dd_part)); */
			}
		}

		if (mdl != nullptr && !same_name) {
			dd_mdl_release(thd, mdl);
			continue;
		}

		ut_ad(same_name);
		break;
	}

	ut_ad(dd_part != nullptr
	      || dd_table->se_private_id() == table_id);
	ut_ad(dd_part == nullptr || dd_table == &dd_part->table());
	ut_ad(dd_part == nullptr || dd_part->se_private_id() == table_id);

	dict_table_t*	ib_table = nullptr;

	dd_table_open_on_dd_obj(
		dc, *dd_table, dd_part, tbl_name, nullptr,
		ib_table, mdl == nullptr/*, table */, thd);

	if (mdl && ib_table == nullptr) {
		dd_mdl_release(thd, mdl);
	}

	return(ib_table);
}

/** Check if access to a table should be refused.
@param[in,out]	table	InnoDB table or partition
@return	error code
@retval	0	on success */
static MY_ATTRIBUTE((warn_unused_result))
int
dd_check_corrupted(dict_table_t*& table)
{

	if (table->is_corrupted()) {
		if (dict_table_is_sdi(table->id) || table->id <= 16) {
			my_error(ER_TABLE_CORRUPT, MYF(0),
				 "", table->name.m_name);
		} else {
			char	db_buf[NAME_LEN + 1];
			char	tbl_buf[NAME_LEN + 1];

			innobase_parse_tbl_name(
				table->name.m_name, db_buf, tbl_buf);
			my_error(ER_TABLE_CORRUPT, MYF(0),
				 db_buf, tbl_buf);
		}
		table = nullptr;
		return(HA_ERR_TABLE_CORRUPT);
	}

	dict_index_t* index = table->first_index();
	if (!dict_table_is_sdi(table->id)
	    && fil_space_get(index->space) == nullptr) {
		// TODO: use index&table name
		my_error(ER_TABLESPACE_MISSING, MYF(0), table->name.m_name);
		table = nullptr;
		return(HA_ERR_TABLESPACE_MISSING);
	}

	/* Ignore missing tablespaces for secondary indexes. */
	while ((index = index->next())) {
		if (!index->is_corrupted()
		    && fil_space_get(index->space) == nullptr) {
			dict_set_corrupted(index);
		}
	}

	return(0);
}

/** Open a persistent InnoDB table based on InnoDB table id, and
held Shared MDL lock on it.
@param[in]	table_id	table identifier
@param[in,out]	thd		current MySQL connection (for mdl)
@param[in,out]	mdl		metadata lock (*mdl set if table_id was found);
@param[in]	dict_lock	dict_sys mutex is held
mdl=NULL if we are resurrecting table IX locks in recovery
@return table
@retval NULL if the table does not exist or cannot be opened */
dict_table_t*
dd_table_open_on_id(
	table_id_t	table_id,
	THD*		thd,
	MDL_ticket**	mdl,
	bool		dict_locked)
{
	dict_table_t*   ib_table;
	const ulint     fold = ut_fold_ull(table_id);

	if (!dict_locked) {
		mutex_enter(&dict_sys->mutex);
	}

	HASH_SEARCH(id_hash, dict_sys->table_id_hash, fold,
		    dict_table_t*, ib_table, ut_ad(ib_table->cached),
		    ib_table->id == table_id);
	if (ib_table == NULL) {
		if (dict_table_is_sdi(table_id)) {
			/* The table is SDI table */
			space_id_t      space_id = dict_sdi_get_space_id(
				table_id);
			uint32_t        copy_num = dict_sdi_get_copy_num(
				table_id);

			/* Create in-memory table oject for SDI table */
			dict_index_t*   sdi_index = dict_sdi_create_idx_in_mem(
				space_id, copy_num, false, 0);

			if (sdi_index == NULL) {
				if (!dict_locked) {
					mutex_exit(&dict_sys->mutex);
				}
				return(NULL);
			}

			ib_table = sdi_index->table;

			ut_ad(ib_table != NULL);
			ib_table->acquire();
			mutex_exit(&dict_sys->mutex);
		} else {
			mutex_exit(&dict_sys->mutex);

			ib_table = dd_table_open_on_id_low(
				thd, mdl, nullptr, table_id);
		}
	} else if (mdl == nullptr || ib_table->is_temporary()
		   || dict_table_is_sdi(ib_table->id)) {
		if (dd_check_corrupted(ib_table)) {
			ut_ad(ib_table == nullptr);
		} else {
			ib_table->acquire();
		}
		mutex_exit(&dict_sys->mutex);
	} else {
		char	db_buf[NAME_LEN + 1];
		char	tbl_buf[NAME_LEN + 1];
		char	full_name[2 * (NAME_LEN + 1)];

		for (;;) {
			innobase_parse_tbl_name(
				ib_table->name.m_name, db_buf, tbl_buf);
			strcpy(full_name, ib_table->name.m_name);

			mutex_exit(&dict_sys->mutex);

			ut_ad(!ib_table->is_temporary());

			if (dd_mdl_acquire(thd, mdl, db_buf, tbl_buf)) {
				return(nullptr);
			}

			/* Re-lookup the table after acquiring MDL. */
			mutex_enter(&dict_sys->mutex);

			HASH_SEARCH(
				id_hash, dict_sys->table_id_hash, fold,
				dict_table_t*, ib_table,
				ut_ad(ib_table->cached),
				ib_table->id == table_id);
			if (ib_table != nullptr) {
				if (dd_check_corrupted(ib_table)) {
                                        ut_ad(ib_table == nullptr);
                                } else {
					ib_table->acquire();
				}
			}

			mutex_exit(&dict_sys->mutex);
			break;
		}

		ut_ad(*mdl != nullptr);

		if (ib_table == nullptr) {
			ib_table = dd_table_open_on_id_low(
				thd, mdl, full_name, table_id);

			if (ib_table == nullptr && *mdl != nullptr) {
				dd_mdl_release(thd, mdl);
			}
		}
	}


	if (ib_table != nullptr) {
		if (table_id > 16 && !dict_table_is_sdi(table_id)
		    && !ib_table->ibd_file_missing
		    && !ib_table->is_fts_aux()) {
			if (!ib_table->stat_initialized) {
				dict_stats_init(ib_table);
			}
			ut_ad(ib_table->stat_initialized);
		}
		ut_ad(ib_table->n_ref_count > 0);
		MONITOR_INC(MONITOR_TABLE_REFERENCE);
	}

	if (dict_locked) {
		mutex_enter(&dict_sys->mutex);
	}
	return(ib_table);
}

/** Set the discard flag for a dd table.
@param[in,out]	thd	current thread
@param[in]	name	InnoDB table name
@param[in]	discard	discard flag
@retval false if fail. */
bool
dd_table_set_discard_flag(
	THD*			thd,
	const char*		name,
	bool			discard)
{
	char			db_buf[NAME_LEN + 1];
	char			tbl_buf[NAME_LEN + 1];
	MDL_ticket*		mdl;
	const dd::Table*	dd_table = nullptr;
	dd::Table*		new_dd_table = nullptr;
	bool			ret = false;

	DBUG_ENTER("dd_table_set_discard_flag");
	ut_ad(thd == current_thd);
#ifdef UNIV_DEBUG
	btrsea_sync_check       check(false);
	ut_ad(!sync_check_iterate(check));
#endif
	ut_ad(!srv_is_being_shutdown);

	innobase_parse_tbl_name(name, db_buf, tbl_buf);

	if (dd_mdl_acquire(thd, &mdl, db_buf, tbl_buf)) {
		DBUG_RETURN(false);
	}

	dd::cache::Dictionary_client*	client = dd::get_dd_client(thd);
	dd::cache::Dictionary_client::Auto_releaser	releaser(client);

	if (!client->acquire(db_buf, tbl_buf, &dd_table)
	    && dd_table != nullptr) {
		if (dd_table->se_private_id() != dd::INVALID_OBJECT_ID) {
			ut_ad(dd_table->partitions().empty());
			/* Clone the dd table object. The clone is owned here,
			and must be deleted eventually. */
			/* Acquire the new dd tablespace for modification */
			if (client->acquire_for_modification(
					db_buf, tbl_buf, &new_dd_table)) {
				ut_a(false);
			}
			new_dd_table->table().options().set_bool("discard",
								 discard);
			client->update(new_dd_table);
			ret = true;
		} else {
			ret = false;
		}
	} else {
		ret = false;
	}

	dd_mdl_release(thd, &mdl);

	DBUG_RETURN(ret);
}

/** Open an internal handle to a persistent InnoDB table by name.
@param[in,out]	thd		current thread
@param[out]	mdl		metadata lock
@param[in]	name		InnoDB table name
@param[in]	dict_locked	has dict_sys mutex locked
@param[in]	ignore_err	whether to ignore err
@return handle to non-partitioned table
@retval NULL if the table does not exist */
dict_table_t*
dd_table_open_on_name(
	THD*			thd,
	MDL_ticket**		mdl,
	const char*		name,
	bool			dict_locked,
	ulint			ignore_err)
{
	DBUG_ENTER("dd_table_open_on_name");

#ifdef UNIV_DEBUG
	btrsea_sync_check       check(false);
	ut_ad(!sync_check_iterate(check));
#endif
	ut_ad(!srv_is_being_shutdown);

	char	db_buf[NAME_LEN + 1];
	char	tbl_buf[NAME_LEN + 1];

	bool	skip_mdl = !(thd && mdl);
	dict_table_t*	table = nullptr;

	/* Get pointer to a table object in InnoDB dictionary cache.
	For intrinsic table, get it from session private data */
	if (thd) {
		table = thd_to_innodb_session(
			thd)->lookup_table_handler(name);
	}

	if (table != nullptr) {
		table->acquire();
		DBUG_RETURN(table);
	}

	innobase_parse_tbl_name(name, db_buf, tbl_buf);

	if (!skip_mdl && dd_mdl_acquire(thd, mdl, db_buf, tbl_buf)) {
		DBUG_RETURN(nullptr);
	}

	if (!dict_locked) {
		mutex_enter(&dict_sys->mutex);
	}

	table = dict_table_check_if_in_cache_low(name);

	if (table != nullptr) {
		table->acquire();
		if (!dict_locked) {
			mutex_exit(&dict_sys->mutex);
		}
		DBUG_RETURN(table);
	}

	mutex_exit(&dict_sys->mutex);

	const dd::Table*		dd_table = nullptr;
	dd::cache::Dictionary_client*	client = dd::get_dd_client(thd);
	dd::cache::Dictionary_client::Auto_releaser	releaser(client);

	if (client->acquire(db_buf, tbl_buf, &dd_table)
	    || dd_table == nullptr) {
		table = nullptr;
	} else {
		if (dd_table->se_private_id() == dd::INVALID_OBJECT_ID) {
			/* This must be a partitioned table. */
			ut_ad(!dd_table->partitions().empty());
			table = nullptr;
		} else {
			ut_ad(dd_table->partitions().empty());
			dd_table_open_on_dd_obj(
				client, *dd_table, nullptr, name,
				nullptr, table, skip_mdl, thd);
		}
	}

	if (table && table->is_corrupted()
	    && !(ignore_err & DICT_ERR_IGNORE_CORRUPT)) {
		mutex_enter(&dict_sys->mutex);
		table->release();
		dict_table_remove_from_cache(table);
		table = NULL;
		mutex_exit(&dict_sys->mutex);
	}

	if (table == nullptr && mdl) {
		dd_mdl_release(thd, mdl);
		*mdl = nullptr;
	}

	if (dict_locked) {
		mutex_enter(&dict_sys->mutex);
	}

        DBUG_RETURN(table);
}

/** Close an internal InnoDB table handle.
@param[in,out]	table		InnoDB table handle
@param[in,out]	thd		current MySQL connection (for mdl)
@param[in,out]	mdl		metadata lock (will be set NULL)
@param[in]	dict_locked	whether we hold dict_sys mutex */
void
dd_table_close(
	dict_table_t*	table,
	THD*		thd,
	MDL_ticket**	mdl,
	bool		dict_locked)
{
	dict_table_close(table, dict_locked, false);

	const bool is_temp = table->is_temporary();

	MONITOR_DEC(MONITOR_TABLE_REFERENCE);

	if (!is_temp && mdl != nullptr
	    && (*mdl != reinterpret_cast<MDL_ticket*>(-1))) {
		dd_mdl_release(thd, mdl);
	}
}

/** Update dd tablespace for rename
@param[in]	dd_space_id	dd tablespace id
@param[in]	new_path	new data file path
@retval true if fail. */
bool
dd_tablespace_update_for_rename(
	dd::Object_id		dd_space_id,
	const char*		new_path)
{
	dd::Tablespace*		dd_space = nullptr;
	dd::Tablespace*		new_space = nullptr;
	bool			ret = false;
	THD*			thd = current_thd;

	DBUG_ENTER("dd_tablespace_update_for_rename");
#ifdef UNIV_DEBUG
	btrsea_sync_check       check(false);
	ut_ad(!sync_check_iterate(check));
#endif
	ut_ad(!srv_is_being_shutdown);
	ut_ad(new_path != NULL);

	dd::cache::Dictionary_client*	client = dd::get_dd_client(thd);
	dd::cache::Dictionary_client::Auto_releaser	releaser(client);

	/* Get the dd tablespace */

	if (client->acquire_uncached_uncommitted<dd::Tablespace>(
			dd_space_id, &dd_space)) {
		ut_a(false);
	}

	ut_a(dd_space != NULL);
	/* Acquire mdl share lock */
	if (dd::acquire_exclusive_tablespace_mdl(
		    thd, dd_space->name().c_str(), false)) {
		ut_a(false);
	}

	/* Acquire the new dd tablespace for modification */
	if (client->acquire_for_modification<dd::Tablespace>(
			dd_space_id, &new_space)) {
		ut_a(false);
	}

	ut_ad(new_space->files().size() == 1);
	dd::Tablespace_file*	dd_file = const_cast<
		dd::Tablespace_file*>(*(new_space->files().begin()));
	dd_file->set_filename(new_path);
	bool fail = client->update(new_space);
	ut_a(!fail);

	DBUG_RETURN(ret);
}
