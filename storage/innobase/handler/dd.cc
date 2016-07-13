/*****************************************************************************

Copyright (c) 2005, 2016, Oracle and/or its affiliates. All Rights Reserved.

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

/**
@file handler/dd.cc
Data dictionary interface */

/* Include necessary SQL headers */
#include "ha_prototypes.h"
#include <current_thd.h>
#include <mysqld.h>
#include <sql_class.h>
#include <sql_table.h>
#include "derror.h"

#include "dd/dd.h"
#include "dd/dictionary.h"
#include "dd/cache/dictionary_client.h"
#include "dd/properties.h"
#include "dd/types/schema.h"
#include "dd/types/table.h"
#include "dd/types/column.h"
#include "dd/types/index.h"
#include "dd/types/index_element.h"
#include "dd/types/partition.h"
#include "dd/types/partition_index.h"
#include "dd/types/tablespace_file.h"
#include "dd_table_share.h"

#include "partition_info.h"
#include "ha_innopart.h"

#include "dd.h"
#include "tablespace.h"
#include "table_dropper.h"
#include "table_factory.h"

#include "dict0stats.h"
#include "dict0stats_bg.h"
#include "fsp0sysspace.h"
#include "log0log.h"
#include "trx0trx.h"
#include "srv0mon.h"
#include "fts0priv.h"
#include "ut0new.h"

#include <algorithm>

extern bool	mysql_sysvar_strict_mode;

#ifdef UNIV_DEBUG
/** Determine if the current thread is holding MDL on the table.
@param[in]	table		table or partition
@param[in]	exclusive	whether to check for exclusive MDL
@retval	true	if the MDL is being held, or the table is temporary
@retval	false	if the MDL is not being held */
bool
dd_has_mdl(const dict_table_t* table, bool exclusive)
{
	if (table->is_temporary()) {
		return(true);
	}

	THD*			thd = current_thd;
	const table_name_t&	name = table->name;

	return(exclusive
	       ? dd::has_exclusive_table_mdl(thd, name.db(), name.table())
	       : dd::has_shared_table_mdl(thd, name.db(), name.table()));
}
#endif /* UNIV_DEBUG */

/** CREATE an InnoDB table.
@param[in]	form		table structure
@param[in]	create_info	more information on the table
@param[in,out]	dd_table	data dictionary cache object,
				or NULL if internal temporary table
@param[in]	implicit	true=imply TABLESPACE=innodb_file_per_table
@return	error number
@retval 0 on success */
int
ha_innobase::create_impl(
	const TABLE*	form,
	HA_CREATE_INFO*	create_info,
	dd::Table*	dd_table,
	bool		implicit)
{
	DBUG_ENTER("ha_innobase::create_impl");

	THD*		thd	= ha_thd();
	trx_t*		trx	= check_trx_exists(thd);
	InnoDB_share*	share;

	if (dd_table == nullptr) {
		ut_ad((HA_LEX_CREATE_INTERNAL_TMP_TABLE
		       | HA_LEX_CREATE_TMP_TABLE)
		      == create_info->options);
		ut_ad(create_info->used_fields == 0);
		ut_ad(form->found_next_number_field == nullptr);

		lock_shared_ha_data();
		share = get_share();
		unlock_shared_ha_data();
		if (share == nullptr) {
			DBUG_RETURN(HA_ERR_OUT_OF_MEM);
		}
	} else {
		if (const char* df = create_info->data_file_name) {
			dd_table->se_private_data().set(
				dd_table_key_strings[DD_TABLE_DATA_DIRECTORY],
				std::string(df, std::max(dirname_length(df),
							 size_t(1)) - 1));
		}

		if (dict_sys_t::hardcoded(dd_table->se_private_id())) {
			ut_ad(trx->id == 0);
			ut_ad(create_info->data_file_name == nullptr);
			ut_ad(create_info->index_file_name == nullptr);
		} else {
			if (form->found_next_number_field != nullptr) {
				dd_set_autoinc(
					dd_table->se_private_data(),
					create_info->auto_increment_value);
			}

			if (!(create_info->options
			      & HA_LEX_CREATE_TMP_TABLE)) {
				/* Ensure that a read-write
				transaction exists, so that we will be
				able to assign trx_id for the
				row_prebuilt_t::index_usable check. */
				innobase_register_trx(ht, thd, trx);
				trx_start_if_not_started(trx, true);
				if (trx->id == 0) {
					my_error(ER_READ_ONLY_MODE, MYF(0));
					DBUG_RETURN(HA_ERR_TABLE_READONLY);
				}
			}
		}

		share = nullptr;
	}

	table_factory	conv(thd, form, create_info, implicit);

	if (int error = conv.create_table(dd_table, strict_mode(thd), trx)) {
		DBUG_RETURN(error);
	} else if (share != nullptr) {
		share->set_table(conv.table());
		trx->mysql_n_internal++;
	}

	DBUG_RETURN(0);
}

/** Create an InnoDB table.
@param[in]	form		Table format; columns and index information.
@param[in]	create_info	Create info (including create statement string)
@param[in,out]	dd_table	data dictionary cache object
@return	error number
@retval 0 on success */
int
ha_innobase::create(
	const char*,
	TABLE*		form,
	HA_CREATE_INFO*	create_info,
	dd::Table*	dd_table)
{
	/* Determine if this CREATE TABLE will be making an implicit
	tablespace.  Note that innodb_file_per_table could be changed
	while creating the table. So we read the current value here
	and make all further decisions based on this. */
	return(create_impl(form, create_info, dd_table, srv_file_per_table));
}

/** CREATE a partitioned InnoDB table.
@param[in]	form		table structure
@param[in]	create_info	more information on the table
@param[in,out]	dd_table	data dictionary cache object
@param[in]	implicit	true=imply TABLESPACE=innodb_file_per_table
@return error number
@retval	0 on success */
int
ha_innopart::create_impl(
	const TABLE*	form,
	HA_CREATE_INFO*	create_info,
	dd::Table*	dd_table,
	bool		implicit)
{
	DBUG_ENTER("ha_innopart::create_impl");
	ut_ad(dd_table != nullptr);
	ut_ad(!(HA_LEX_CREATE_INTERNAL_TMP_TABLE & create_info->options));
	ut_ad(create_info != nullptr);
	ut_ad(table_share != nullptr);
	ut_ad(dd_table->se_private_id() == dd::INVALID_OBJECT_ID);

	THD*		thd = ha_thd();
	trx_t*		trx = check_trx_exists(thd);

	/* Not allowed to create temporary partitioned tables. */
	if ((create_info->options & HA_LEX_CREATE_TMP_TABLE) != 0) {
		my_error(ER_PARTITION_NO_TEMPORARY, MYF(0));
		DBUG_RETURN(HA_ERR_INTERNAL_ERROR);
	}

	/* Ensure that a read-write transaction exists, so that
	we will be able to assign index->trx_id = trx->id and also
	DD_INDEX_TRX_ID, for the row_prebuilt_t::index_usable check. */
	trx_start_if_not_started(trx, true);
	innobase_register_trx(ht, thd, trx);

	if (trx->id == 0) {
		my_error(ER_READ_ONLY_MODE, MYF(0));
		DBUG_RETURN(HA_ERR_TABLE_READONLY);
	}

	if (create_info->data_file_name != nullptr) {
		dd_table->se_private_data().set(
			dd_table_key_strings[DD_TABLE_DATA_DIRECTORY],
			std::string(create_info->data_file_name,
				    std::max(size_t(1), dirname_length(
						     create_info
						     ->data_file_name)) - 1));
	}

	if (form->found_next_number_field) {
		dd_set_autoinc(dd_table->se_private_data(),
			       create_info->auto_increment_value);
	}

	/* Create each partition or sub-partition */
	// WL#7016 TODO: Do not add the partitions to the cache before commit!
	ut_d(uint total_created = 0);
	const bool	strict	= strict_mode(thd);
	for (dd::Partition* dd_part : *dd_table->partitions()) {
		if (!dd_part_is_stored(dd_part)) {
			continue;
		}

		table_factory conv(thd, form, create_info, implicit);
		dict_table_t::filename_t filename;
		if (int error = conv.create_part(dd_part,
						 form->s->table_name.str,
						 strict, trx, filename)) {
			DBUG_RETURN(error);
		}
		ut_d(total_created++);
	}

	ut_ad(total_created == m_tot_parts);

	/* Tell the InnoDB server that there might be work for
	utility threads */
	srv_active_wake_master_thread();

	DBUG_RETURN(0);
}

/** Look up a column in a table using the system_charset_info collation.
@param[in]	dd_table	data dictionary table
@param[in]	name		column name
@return	the column
@retval	nullptr	if not found */
static
const dd::Column*
dd_find_column(dd::Table* dd_table, const char* name)
{
	for (const dd::Column* c : dd_table->columns()) {
		if (!my_strcasecmp(system_charset_info,
				   c->name().c_str(), name)) {
			return(c);
		}
	}
	return(nullptr);
}

/** Check if a column is the only column in an index.
@param[in]	index	data dictionary index
@param[in]	column	the column to look for
@return	whether the column is the only column in the index */
static
bool
dd_is_only_column(const dd::Index* index, const dd::Column* column)
{
	return(index->elements().size() == 1
	       && &(*index->elements().begin())->column() == column);
}

/** Add a hidden index element at the end.
@param[in,out]	index	created index metadata
@param[in]	column	column of the index */
static
void
dd_add_hidden_element(dd::Index* index, const dd::Column* column)
{
	dd::Index_element* e = index->add_element(
		const_cast<dd::Column*>(column));
	e->set_hidden(true);
	e->set_order(dd::Index_element::ORDER_ASC);
}

/** Initialize a hidden unique B-tree index.
@param[in,out]	index	created index metadata
@param[in]	name	name of the index
@param[in]	column	column of the index
@return the initialized index */
static
dd::Index*
dd_set_hidden_unique_index(
	dd::Index*		index,
	const char*		name,
	const dd::Column*	column)
{
	index->set_name(name);
	index->set_hidden(true);
	index->set_algorithm(dd::Index::IA_BTREE);
	index->set_type(dd::Index::IT_UNIQUE);
	index->set_engine(ha_innobase::hton_name);
	dd_add_hidden_element(index, column);
	return(index);
}

/** Add a hidden column when creating a table.
@param[in,out]	dd_table	table containing user columns and indexes
@param[in]	name		hidden column name
@param[in]	length		length of the column, in bytes
@return the added column, or NULL if there already was a column by that name */
static
dd::Column*
dd_add_hidden_column(
	dd::Table*	dd_table,
	const char*	name,
	uint		length)
{
	if (const dd::Column* c = dd_find_column(dd_table, name)) {
		my_error(ER_WRONG_COLUMN_NAME, MYF(0), c->name().c_str());
		return(nullptr);
	}

	dd::Column* col = dd_table->add_column();
	col->set_hidden(true);
	col->set_name(name);
	col->set_type(dd::enum_column_types::STRING);
	col->set_nullable(false);
	col->set_char_length(length);
	col->set_collation_id(my_charset_bin.number);

	return(col);
}

/** Add hidden columns and indexes to an InnoDB table definition.
@param[in,out]	thd		session
@param[in,out]	dd_table	data dictionary cache object
@return error number
@retval 0 on success */
int
ha_innobase::get_extra_columns_and_keys(THD* thd, dd::Table* dd_table)
{
	DBUG_ENTER("ha_innobase::get_extra_columns_and_keys");
	dd::Index*		primary			= nullptr;
	bool			has_fulltext		= false;
	const dd::Index*	fts_doc_id_index	= nullptr;

	for (dd::Index* i : *dd_table->indexes()) {
		/* The name "PRIMARY" is reserved for the PRIMARY KEY */
		ut_ad((i->type() == dd::Index::IT_PRIMARY)
		      == !my_strcasecmp(system_charset_info, i->name().c_str(),
					primary_key_name));

		if (!my_strcasecmp(system_charset_info,
				   i->name().c_str(), FTS_DOC_ID_INDEX_NAME)) {
			ut_ad(!fts_doc_id_index);
			ut_ad(i->type() != dd::Index::IT_PRIMARY);
			fts_doc_id_index = i;
		}

		switch (i->algorithm()) {
		case dd::Index::IA_SE_SPECIFIC:
			ut_ad(0);
			break;
		case dd::Index::IA_HASH:
			/* This is currently blocked
			by ha_innobase::is_index_algorithm_supported(). */
			ut_ad(0);
			break;
		case dd::Index::IA_RTREE:
			if (i->type() == dd::Index::IT_SPATIAL) {
				continue;
			}
			ut_ad(0);
			break;
		case dd::Index::IA_BTREE:
			switch (i->type()) {
			case dd::Index::IT_PRIMARY:
				ut_ad(!primary);
				ut_ad(i == dd_first_index(dd_table));
				primary = i;
				continue;
			case dd::Index::IT_UNIQUE:
				if (primary == nullptr
				    && dd_index_is_candidate_key(i)) {
					primary = i;
					ut_ad(i == dd_first_index(dd_table));
				}
				continue;
			case dd::Index::IT_MULTIPLE:
				continue;
			}
			break;
		case dd::Index::IA_FULLTEXT:
			if (i->type() == dd::Index::IT_FULLTEXT) {
				has_fulltext = true;
				continue;
			}
			ut_ad(0);
			break;
		}

		my_error(ER_UNSUPPORTED_INDEX_ALGORITHM,
			 MYF(0), i->name().c_str());
		DBUG_RETURN(ER_UNSUPPORTED_INDEX_ALGORITHM);
	}

	if (has_fulltext) {
		/* Add FTS_DOC_ID_INDEX(FTS_DOC_ID) if needed */
		const dd::Column* fts_doc_id = dd_find_column(
			dd_table, FTS_DOC_ID_COL_NAME);

		if (fts_doc_id_index) {
			switch (fts_doc_id_index->type()) {
			case dd::Index::IT_PRIMARY:
				/* PRIMARY!=FTS_DOC_ID_INDEX */
				ut_ad(!"wrong fts_doc_id_index");
				/* fall through */
			case dd::Index::IT_UNIQUE:
				/* We already checked for this. */
				ut_ad(fts_doc_id_index->algorithm()
				      == dd::Index::IA_BTREE);
				if (dd_is_only_column(fts_doc_id_index,
						      fts_doc_id)) {
					break;
				}
				/* fall through */
			case dd::Index::IT_MULTIPLE:
			case dd::Index::IT_FULLTEXT:
			case dd::Index::IT_SPATIAL:
				my_error(ER_INNODB_FT_WRONG_DOCID_INDEX,
					 MYF(0),
					 fts_doc_id_index->name().c_str());
				push_warning(
					thd,
					Sql_condition::SL_WARNING,
					ER_WRONG_NAME_FOR_INDEX,
					" InnoDB: Index name "
					FTS_DOC_ID_INDEX_NAME " is reserved"
					" for UNIQUE INDEX("
					FTS_DOC_ID_COL_NAME ") for "
					" FULLTEXT Document ID indexing.");
				DBUG_RETURN(ER_INNODB_FT_WRONG_DOCID_INDEX);
			}
			ut_ad(fts_doc_id);
		}

		if (fts_doc_id) {
			if (fts_doc_id->type()
			    != dd::enum_column_types::LONGLONG
			    || !fts_doc_id->is_unsigned()
			    || fts_doc_id->is_nullable()
			    || fts_doc_id->name() != FTS_DOC_ID_COL_NAME) {
				my_error(ER_INNODB_FT_WRONG_DOCID_COLUMN,
					 MYF(0),
					 fts_doc_id->name().c_str());
				push_warning(
					thd,
					Sql_condition::SL_WARNING,
					ER_WRONG_COLUMN_NAME,
					" InnoDB: Column name "
					FTS_DOC_ID_COL_NAME " is reserved for"
					" FULLTEXT Document ID indexing.");
				DBUG_RETURN(ER_INNODB_FT_WRONG_DOCID_COLUMN);
			}
		} else {
			/* Add hidden FTS_DOC_ID column */
			dd::Column* col = dd_table->add_column();
			col->set_hidden(true);
			col->set_name(FTS_DOC_ID_COL_NAME);
			col->set_type(dd::enum_column_types::LONGLONG);
			col->set_nullable(false);
			col->set_unsigned(true);
			fts_doc_id = col;
		}

		ut_ad(fts_doc_id);

		if (fts_doc_id_index == nullptr
		    && (primary == nullptr
			|| !dd_is_only_column(primary, fts_doc_id))) {
			dd_set_hidden_unique_index(dd_table->add_index(),
						   FTS_DOC_ID_INDEX_NAME,
						   fts_doc_id);
		}
	}

	if (primary == nullptr) {
		dd::Column* db_row_id = dd_add_hidden_column(
			dd_table, "DB_ROW_ID", DATA_ROW_ID_LEN);

		if (db_row_id == nullptr) {
			DBUG_RETURN(ER_WRONG_COLUMN_NAME);
		}

		primary = dd_set_hidden_unique_index(
			dd_table->add_first_index(),
			primary_key_name,
			db_row_id);
	}

	/* Add PRIMARY KEY columns to each secondary index, including:
	1. all PRIMARY KEY column prefixes
	2. full PRIMARY KEY columns which don't exist in the secondary index */

	std::vector<const dd::Index_element*, ut_allocator<dd::Index_element*>>
		pk_elements;

	for (dd::Index* index : *dd_table->indexes()) {
		if (index == primary) {
			continue;
		}

		pk_elements.clear();
		for (const dd::Index_element* e : primary->elements()) {
			if (dd_index_element_is_prefix(e)
			    || std::search_n(index->elements().begin(),
					     index->elements().end(), 1, e,
					     [](const dd::Index_element* ie,
						const dd::Index_element* e) {
						     return(&ie->column()
							    == &e->column());
					     }) == index->elements().end()) { 
				pk_elements.push_back(e);
			}
		}

		for (const dd::Index_element* e : pk_elements) {
			auto ie = index->add_element(
				const_cast<dd::Column*>(&e->column()));
			ie->set_hidden(true);
			ie->set_order(e->order());
		}
	}

	/* Add the InnoDB system columns DB_TRX_ID, DB_ROLL_PTR. */
	dd::Column* db_trx_id = dd_add_hidden_column(
		dd_table, "DB_TRX_ID", DATA_TRX_ID_LEN);
	if (db_trx_id == nullptr) {
		DBUG_RETURN(ER_WRONG_COLUMN_NAME);
	}

	dd::Column* db_roll_ptr = dd_add_hidden_column(
		dd_table, "DB_ROLL_PTR", DATA_ROLL_PTR_LEN);
	if (db_roll_ptr == nullptr) {
		DBUG_RETURN(ER_WRONG_COLUMN_NAME);
	}

	dd_add_hidden_element(primary, db_trx_id);
	dd_add_hidden_element(primary, db_roll_ptr);

	/* Add all non-virtual columns to the clustered index,
	unless they already part of the PRIMARY KEY. */

	for (const dd::Column* c : dd_table->columns()) {
		if (c->is_hidden() || c->is_virtual()) {
			continue;
		}

		if (std::search_n(primary->elements().begin(),
				  primary->elements().end(), 1,
				  c, [](const dd::Index_element* e,
					const dd::Column* c)
				  {
					  return(!dd_index_element_is_prefix(e)
						 && &e->column() == c);
				  })
		    == primary->elements().end()) {
			dd_add_hidden_element(primary, c);
		}
	}

	DBUG_RETURN(0);
}

/** Prepare to truncate a table or partition.
@tparam		Table		dd::Table or dd::Partition
@param[in,out]	client		data dictionary client
@param[in,out]	dd_table	Global Data Dictionary metadata
@param[in]	dd_space_id	dd_first_index(dd_table)->tablespace_id()
@param[in]	table_name	table name (for diagnostics)
@param[out]	dd_space	TABLESPACE=innodb_file_per_table, or NULL
@return	handler error code
@retval	0				on success
@retval	HA_ERR_TABLESPACE_MISSING	if the tablespace is not found */
template<typename Table>
inline
int
truncate_prepare(
	dd::cache::Dictionary_client*	client,
	Table*				dd_table,
	dd::Object_id			dd_space_id,
	const table_name_t&		table_name,
	const dd::Tablespace**		dd_space)
{
	DBUG_ENTER("truncate_prepare");
	ut_ad(dd_space_id == dd_first_index(dd_table)->tablespace_id());

	dd::Tablespace*	space;

	if (dd_get_implicit_tablespace(client, dd_space_id, table_name,
				       space)) {
		DBUG_RETURN(HA_ERR_TABLESPACE_MISSING);
	}

	*dd_space = space;
#if 0//WL#7016 TODO
	if (space != nullptr) {
		/* rename the old file, because we
		will not delete it before commit */
	}
#endif

	dd_table->set_se_private_id(dd::INVALID_OBJECT_ID);
	for (auto dd_index : *dd_table->indexes()) {
		dd_index->se_private_data().clear();
	}
	DBUG_RETURN(0);
}

/** TRUNCATE (DROP and CREATE) an InnoDB table.
@param[in,out]	dd_table	data dictionary table
@return	error number
@retval 0 on success */
int
ha_innobase::truncate(dd::Table* dd_table)
{
	DBUG_ENTER("ha_innobase::truncate");
	/* The table should have been opened in ha_innobase::open(). */
	ut_ad(dict_table_is_intrinsic(m_prebuilt->table)
	      == (dd_table == nullptr));
	ut_ad(dict_table_is_temporary(m_prebuilt->table)
	      || dd::has_exclusive_table_mdl(
		      m_user_thd,
		      m_prebuilt->table->name.db(),
		      m_prebuilt->table->name.table()));
	ut_ad(table->s == table_share);

	if (dict_sys_t::hardcoded(m_prebuilt->table->id)) {
		ut_ad(!m_prebuilt->table->is_temporary());
		my_error(ER_NOT_ALLOWED_COMMAND, MYF(0));
		DBUG_RETURN(HA_ERR_UNSUPPORTED);
	}

	ut_ad(m_prebuilt->table->n_ref_count == 1);

	if (dd_table == nullptr) {
		DBUG_RETURN(delete_all_rows());
	}

	ut_ad((table->found_next_number_field != nullptr)
	      == dict_table_has_autoinc_col(m_prebuilt->table));
	if (table->found_next_number_field) {
		dd_set_autoinc(dd_table->se_private_data(), 0);
	}

	/* Note: ALTER TABLE on temporary tables (which is always
	ALGORITHM=COPY) will not call rename_table(). Therefore, we
	may have a name mismatch. The dd_table->name() is the
	user-specified table name, so it should be correct again after
	TRUNCATE. */
	ut_ad(dd_table->name() == m_prebuilt->table->name.table()
	      || m_prebuilt->table->is_temporary());

	if (high_level_read_only) {
		DBUG_RETURN(HA_ERR_TABLE_READONLY);
	}

	if (!dict_table_is_temporary(m_prebuilt->table)) {
		innobase_register_trx(ht, m_user_thd, m_prebuilt->trx);
	}

	dd::cache::Dictionary_client*	client = dd::get_dd_client(m_user_thd);
	dd::cache::Dictionary_client::Auto_releaser releaser(client);
	const dd::Object_id		dd_space_id
		= dd_first_index(dd_table)->tablespace_id();
	const dd::Tablespace*		implicit	= nullptr;
	int				error;

	/* TODO: Remove info, and adjust table_factory::create_table_def() */
	HA_CREATE_INFO	info;
	memset(&info, 0, sizeof info);
	update_create_info_from_table(&info, table);
	info.key_block_size = table_share->key_block_size;
	info.options = m_prebuilt->table->is_temporary()
		? HA_LEX_CREATE_TMP_TABLE : 0;

	error = truncate_prepare(client, dd_table, dd_space_id,
				 m_prebuilt->table->name, &implicit);

	if (error == 0) {
		table_dropper	dropper(m_prebuilt->table, true, false);
		if (dict_table_is_temporary(m_prebuilt->table)) {
			dropper.drop_temporary();
		} else {
			error = dropper.drop(client, m_prebuilt->trx,
					     dd_space_id);
			ut_ad(error == 0);//WL#7016 should allow errors
		}
	}

	dict_table_t*	new_table = nullptr;

	if (error == 0) {
		/* Ensure that a read-write transaction exists, so that
		we will be able to assign index->trx_id = trx->id. */
		trx_start_if_not_started(m_prebuilt->trx, true);

		table_factory	conv(m_user_thd, table, &info, implicit);
		error = conv.create_table(dd_table, true, m_prebuilt->trx);
		new_table = conv.table();
		ut_ad((new_table != nullptr) == (error == 0));
		ut_ad(error == 0);//WL#7016 should allow errors
	}

	delete implicit;

#if 1//WL#7016 TODO: do not update the cache before commit
	if (new_table != nullptr) {
		trx_t*	trx = m_prebuilt->trx;
		row_prebuilt_free(m_prebuilt);
		m_prebuilt = row_create_prebuilt(new_table, table);
		m_prebuilt->trx = trx;
		ut_ad(!dict_table_has_autoinc_col(new_table)
		      || new_table->autoinc == 1);
		mutex_enter(&dict_sys->mutex);
		ut_ad(new_table->get_ref_count() == 0);
		new_table->acquire();
		mutex_exit(&dict_sys->mutex);
	}
#endif

	DBUG_RETURN(error);
}

/** TRUNCATE (DROP and CREATE) a partitioned table.
@param[in,out]	dd_table	table metadata
@return error number
@retval 0 on success */
int
ha_innopart::truncate(dd::Table* dd_table)
{
	DBUG_ENTER("ha_innopart::truncate");
	ut_ad(m_part_info->num_partitions_used() == m_tot_parts);
	DBUG_RETURN(truncate_partition_low(dd_table));
}

/** ALTER TABLE...TRUNCATE PARTITION. Also it's called by ha_innopart::truncate
@param[in,out]	dd_table	table metadata
@return	error number
@retval	0	on success */
int
ha_innopart::truncate_partition_low(dd::Table* dd_table)
{
	DBUG_ENTER("ha_innopart::truncate_partition_low");
	ut_ad(dd_table != nullptr);
	ut_ad(table->s == table_share);
	ut_ad(dd_table->name() == table_share->table_name.str);
	ut_ad(dd::has_exclusive_table_mdl(
		      m_user_thd, m_prebuilt->table->name.db(),
		      m_prebuilt->table->name.table()));
	ut_ad(m_prebuilt->table->n_ref_count == 1);
	ut_ad(m_part_info->num_partitions_used() > 0);
	ut_ad(m_part_info->num_partitions_used() <= m_tot_parts);
	ut_ad((table->found_next_number_field != nullptr)
	      == dict_table_has_autoinc_col(m_prebuilt->table));

	/* TRUNCATE TABLE and ALTER TABLE...TRUNCATE PARTITION ALL
	must reset the AUTO_INCREMENT sequence, but
	TRUNCATE PARTITION of some partitions should not affect it. */
	const ib_uint64_t autoinc = table->found_next_number_field
		&& m_part_info->num_partitions_used() < m_tot_parts
		? m_part_share->next_auto_inc_val : 1;
	if (table->found_next_number_field) {
		dd_set_autoinc(dd_table->se_private_data(), autoinc);
	}

	innobase_register_trx(ht, m_user_thd, m_prebuilt->trx);

	if (high_level_read_only) {
		DBUG_RETURN(HA_ERR_TABLE_READONLY);
	}

	dd::cache::Dictionary_client*	client = dd::get_dd_client(m_user_thd);
	dd::cache::Dictionary_client::Auto_releaser releaser(client);
	uint				i = 0;

	/* TODO: Remove info, and adjust table_factory::create_table_def() */
	HA_CREATE_INFO			info;
	update_create_info_from_table(&info, table);
	memset(&info, 0, sizeof info);
	update_create_info_from_table(&info, table);
	info.key_block_size = table_share->key_block_size;
	/* Ensure that a read-write transaction exists, so that
	we will be able to assign index->trx_id = trx->id. */
	trx_start_if_not_started(m_prebuilt->trx, true);

	for (dd::Partition* dd_part : *dd_table->partitions()) {
		if (!dd_part_is_stored(dd_part)) {
			continue;
		}

		ut_d(dict_table_t* part = m_part_share->get_table_part(i));
		ut_ad(part->n_ref_count == 1);
		ut_ad(!dict_table_is_temporary(part));
		ut_ad(dd_table->name() == part->name.table());
		ut_ad(m_prebuilt->table->name == part->name);
//		ut_ad(part->part_name != nullptr);

		if (m_part_info->is_partition_used(i)) {
			dict_table_t* part = m_part_share->get_table_part(i);
			const dd::Tablespace*	implicit	= nullptr;
			int			error;
			const dd::Object_id	dd_space_id
				= dd_first_index(dd_part)->tablespace_id();

			/* TODO: This is now problematic even without WL#7016,
			what if later operations fail? The data cleared
			should be reset */
			error = truncate_prepare(client, dd_part, dd_space_id,
						 part->name, &implicit);
			if (error == 0) {
				error = table_dropper(part, true, false)
					.drop(client, m_prebuilt->trx,
					      dd_space_id);
				ut_ad(error == 0);//WL#7016 should allow errors
			}

			dict_table_t*	new_part = nullptr;

			if (error == 0) {
				table_factory conv(m_user_thd, table,
						   &info, implicit);
				dict_table_t::filename_t filename;
				error = conv.create_part(
					dd_part, table_share->table_name.str,
					true, m_prebuilt->trx, filename);
				new_part = conv.table();
				ut_ad((new_part != nullptr) == (error == 0));
				ut_ad(error == 0);//WL#7016 should allow errors
			}

			delete implicit;

#if 1//WL#7016 TODO: do not update the cache before commit
			if (new_part != nullptr) {
				ut_ad(!dict_table_has_autoinc_col(new_part)
				      || new_part->autoinc == autoinc);
				mutex_enter(&dict_sys->mutex);
				ut_ad(new_part->get_ref_count() == 0);
				new_part->acquire();
				mutex_exit(&dict_sys->mutex);
				m_part_share->set_table_part(i, new_part);
				if (m_prebuilt->table == part) {
					m_prebuilt->table = new_part;
					m_prebuilt->index = new_part
						->first_index();
				}
			}
#endif

			if (error != 0) {
				DBUG_RETURN(error);
			}
		}

		i++;
	}

	ut_ad(i == m_tot_parts);

	/* Even if we did not reset AUTO_INCREMENT, ensure
	that it will be re-read at the next use. */
	if (table->found_next_number_field) {
		lock_auto_increment();
		m_part_share->next_auto_inc_val = 0;
		m_part_share->auto_inc_initialized = false;
		DBUG_EXECUTE_IF("partition_truncate_no_reset",
				m_part_share->auto_inc_initialized = true;);
		unlock_auto_increment();
	}

	DBUG_RETURN(0);
}

/** DROP TABLE
@param[in]	dd_table	data dictionary table
@return	error number
@retval 0 on success */
int
ha_innobase::delete_table(const char*, const dd::Table* dd_table)
{
	DBUG_ENTER("ha_innobase::delete_table");
	ut_ad(dd_table == nullptr
	      || dd_table->partition_type() == dd::Table::PT_NONE);

	if (dd_table != nullptr
	    && dict_sys_t::hardcoded(dd_table->se_private_id())) {
		my_error(ER_NOT_ALLOWED_COMMAND, MYF(0));
		DBUG_RETURN(HA_ERR_UNSUPPORTED);
	}

	dict_table_t*	ib_table	= nullptr;
	InnoDB_share*	share;
	THD*		thd		= ha_thd();
	trx_t*		trx		= check_trx_exists(thd);
	TrxInInnoDB	trx_in_innodb(trx);

	if (table_share != nullptr) {
		lock_shared_ha_data();
		share = static_cast<InnoDB_share*>(get_ha_share_ptr());
		unlock_shared_ha_data();

		if (share != nullptr) {
			ib_table = share->get_table();
		}
	} else {
		share = nullptr;
	}

	if (dd_table == nullptr) {
		ut_ad(share != nullptr);
		ut_ad(ib_table != nullptr);
		ut_ad(dict_table_is_intrinsic(ib_table));
		ut_ad(trx->mysql_n_internal > 0);

		trx->mysql_n_internal--;

		for (dict_index_t* index = ib_table->first_index();
		     index != nullptr;
		     index = index->next()){
			index->last_ins_cur.release();
			index->last_sel_cur.release();
		}

		delete share;
		table_dropper(ib_table, true, true).drop_temporary();
		DBUG_RETURN(0);
	}

	dd::cache::Dictionary_client* client = dd::get_dd_client(thd);
	dd::cache::Dictionary_client::Auto_releaser releaser(client);
	bool	uncached	= false;
	int	error;

	if (share == nullptr) {
		const bool persistent = dd_table->is_persistent();
		if (persistent && high_level_read_only) {
			DBUG_RETURN(HA_ERR_TABLE_READONLY);
		}
		if (table_share == nullptr && persistent) {
			/* This must have been created with CREATE TABLE. */
			error = dd_open_table(client,
					      *dd_table, nullptr, nullptr,
					      &uncached, ib_table);
			/* We should never evict temporary tables.
			Therefore, table_share should never be NULL
			for them. */
			ut_ad(ib_table == nullptr
			      || !dict_table_is_temporary(ib_table));
		} else {
			error = dd_convert_table(
				client, table, nullptr, &uncached,
				ib_table, *dd_table);
		}

		ut_ad((ib_table == nullptr) == (error != 0));
		switch (error) {
		case 0:
			break;
		case HA_ERR_TABLESPACE_MISSING:
			ut_ad(persistent);
			push_warning_printf(
				thd, Sql_condition::SL_WARNING,
				ER_TABLESPACE_MISSING,
				ER_DEFAULT(ER_TABLESPACE_MISSING),
				dd_table->name().c_str());
			dd_tablespace_drop_missing(
				client, dd_first_index(dd_table)
				->tablespace_id());
			/* fall through */
		default:
			/* The metadata is corrupted or the tablespace
			file is not accessible. Either way, the metadata
			cannot be loaded to the InnoDB cache, so we can
			just let the Global DD drop the metadata. */
			DBUG_RETURN(0);
		}

		ut_ad(!dict_table_is_temporary(ib_table) == persistent);
	}

	table_dropper dropper(ib_table,
			      share == nullptr
			      || dict_table_is_intrinsic(ib_table),
			      uncached || dict_table_is_intrinsic(ib_table));

	if (high_level_read_only && !dict_table_is_temporary(ib_table)) {
		ut_ad(share != nullptr);
		ut_ad(!uncached);
		ut_ad(ib_table->n_ref_count == 0);
		DBUG_RETURN(HA_ERR_TABLE_READONLY);
	}

	if (dict_table_is_temporary(ib_table)) {
		dropper.drop_temporary();

		if (dict_table_is_intrinsic(ib_table)) {
			ut_ad(share != nullptr);
			delete share;
		}

		DBUG_RETURN(0);
	} else {
		innobase_register_trx(ht, thd, trx);

		DBUG_RETURN(dropper.drop(client, trx,
					 dd_first_index(dd_table)
					 ->tablespace_id()));
	}
}

/** DROP TABLE
@param[in]	dd_table	data dictionary table
@return	error number
@retval 0 on success */
int
ha_innopart::delete_table(const char*, const dd::Table* dd_table)
{
	DBUG_ENTER("ha_innopart::delete_table");
	ut_ad(dd_table != nullptr);
	ut_ad(dd_table->partition_type() != dd::Table::PT_NONE);
	ut_ad(!dict_sys_t::hardcoded(dd_table->se_private_id()));

	ut_ad(dd_table->is_persistent());
	if (high_level_read_only) {
		DBUG_RETURN(HA_ERR_TABLE_READONLY);
	}

	Ha_innopart_share*	share;
	THD*			thd = ha_thd();
	trx_t*			trx = check_trx_exists(thd);

	innobase_register_trx(ht, thd, trx);

	table_droppers		droppers;

	if (table_share != nullptr) {
		lock_shared_ha_data();
		share = static_cast<Ha_innopart_share*>(get_ha_share_ptr());
		set_ha_share_ptr(nullptr);
		unlock_shared_ha_data();
		ut_ad(share == nullptr || !share->is_closed());
	} else {
		share = nullptr;
	}

	dd::cache::Dictionary_client* client = dd::get_dd_client(thd);
	dd::cache::Dictionary_client::Auto_releaser releaser(client);

	if (share == nullptr || share->is_closed()) {
		for (const dd::Partition* dd_part : dd_table->partitions()) {
			if (!dd_part_is_stored(dd_part)) {
				continue;
			}
			dict_table_t*	ib_table;
			bool		uncached = false;
			int		error = table == nullptr
				? dd_open_table(client,
						*dd_table,
						dd_part, nullptr, &uncached,
						ib_table)
				: dd_convert_part(client, table,
						  &uncached, ib_table,
						  *dd_part, true);
			dd::Object_id	id = dd_first_index(dd_part)
				->tablespace_id();

			ut_ad((ib_table == nullptr) == (error != 0));
			switch (error) {
			case 0:
				break;
			case HA_ERR_TABLESPACE_MISSING:
				push_warning_printf(
					thd, Sql_condition::SL_WARNING,
					ER_TABLESPACE_MISSING,
					ER_DEFAULT(ER_TABLESPACE_MISSING),
					dd_table->name().c_str());
				dd_tablespace_drop_missing(client, id);
				/* fall through */
			default:
				/* The metadata is corrupted or the tablespace
				file is not accessible. Either way, the object
				cannot be loaded to the InnoDB cache, so we can
				just let the Global DD drop the metadata. */
				continue;
			}

			ut_ad(!ib_table->is_temporary());
			ut_ad(ib_table->fts == nullptr);
			droppers.push_back(
				std::make_pair(id, new table_dropper(
						       ib_table, true,
						       uncached)));
		}
	}

	DBUG_RETURN(droppers.drop(client, trx));
}

/** RENAME a TABLE.
@param[in]	old_table		table to rename from
@param[in]	new_table		table to rename to */
int
ha_innobase::rename_table_impl(
	const dd::Table*	old_table,
	const dd::Table*	new_table)
{
	DBUG_ENTER("ha_innobase::rename_table_impl");
	ut_ad(old_table->partition_type() == dd::Table::PT_NONE);
	ut_ad(new_table->partition_type() == dd::Table::PT_NONE);
	ut_ad(old_table->se_private_id() == new_table->se_private_id());
	ut_ad(old_table->se_private_data().raw_string()
	      == new_table->se_private_data().raw_string());

	if (high_level_read_only) {
		my_error(ER_READ_ONLY_MODE, MYF(0));
		DBUG_RETURN(HA_ERR_TABLE_READONLY);
	}

	if (dict_sys_t::hardcoded(new_table->se_private_id())) {
		my_error(ER_NOT_ALLOWED_COMMAND, MYF(0));
		DBUG_RETURN(HA_ERR_UNSUPPORTED);
	}

	dict_table_t*	ib_table;
	int		error;
	THD*		thd		= ha_thd();
	trx_t*		trx		= check_trx_exists(thd);
	bool		uncached	= false;
	bool		need_open	= table == nullptr;

	trx_start_if_not_started(trx, false);
	innobase_register_trx(ht, thd, trx);

	dd::cache::Dictionary_client* client = dd::get_dd_client(thd);
	dd::cache::Dictionary_client::Auto_releaser releaser(client);
	const dd::Schema* old_schema;
	const dd::Schema* new_schema;
	if (client->acquire<dd::Schema>(old_table->schema_id(),
					&old_schema)
	    || client->acquire<dd::Schema>(new_table->schema_id(),
					   &new_schema)) {
		DBUG_RETURN(HA_ERR_INTERNAL_ERROR);
	}

	if (need_open) {
		error = dd_open_table(client, *old_table, nullptr,
				      nullptr, &uncached, ib_table);
	} else {
		lock_shared_ha_data();
		InnoDB_share*	share = static_cast<InnoDB_share*>(
			get_ha_share_ptr());
		unlock_shared_ha_data(); // TODO: remove the lock

		ib_table = share != nullptr ? share->get_table() : nullptr;
		ut_ad(ib_table != nullptr || share == nullptr);
		need_open = ib_table == nullptr;
		error = need_open
			? dd_convert_table(client, table, nullptr,
					   &uncached, ib_table, *old_table)
			: 0;
	}

	ut_ad((ib_table == nullptr) == (error != 0));

	switch (error) {
	case 0:
		break;
	case HA_ERR_TABLESPACE_MISSING:
		my_error(ER_TABLESPACE_MISSING, MYF(0),
			 old_table->name().c_str());
		/* fall through */
	default:
		DBUG_RETURN(error);
	}

	ut_ad(!dict_table_is_temporary(ib_table));
	ut_ad(ib_table->id == old_table->se_private_id());
	char			old_name_buf[2 * (NAME_LEN + 1)];
	char			new_name_buf[2 * (NAME_LEN + 1)];
	ut_ad(ib_table->name.size() <= sizeof old_name_buf);
	ut_ad(old_schema->name() == ib_table->name.db());
	ut_ad(old_table->name() == ib_table->name.table());

	memcpy(old_name_buf, ib_table->name.db(), ib_table->name.size());
	snprintf(new_name_buf, sizeof new_name_buf, "%s%c%s",
		 new_schema->name().c_str(), 0,
		 new_table->name().c_str());
	const table_name_t	old_name(old_name_buf);
	const table_name_t	new_name(new_name_buf);

	ut_ad(old_name == ib_table->name);
	ut_ad(dd::has_exclusive_table_mdl(
		      thd, ib_table->name.db(), ib_table->name.table()));
	ut_ad(dd::has_exclusive_table_mdl(
		      thd, new_name.db(), new_name.table()));

	if (old_name == new_name) {
		/* No change to table name. */
	} else {
		/* TODO: write DDL log and redo log for renaming the
		tablespace file, and rename it in the file system */
		if (uncached) {
			ut_ad(need_open);
			ib_table->name = new_name;
		} else {
			/* WL#7016 TODO: rename in dict_table_t (or rebuild
			the dict_table_t) when the transaction is
			committed. */
			char*	buf = static_cast<char*>(
				mem_heap_dup(ib_table->heap,
					     new_name.db(), new_name.size()));
			ib_table->name.set_names(buf,
						 buf + new_name.db_size());
		}

		if (!dict_table_is_temporary(ib_table)) {
			dd::Tablespace* dd_space;
			if (dd_get_implicit_tablespace(
				    client,
				    dd_first_index(new_table)->tablespace_id(),
				    ib_table->name, dd_space)) {
				error = HA_ERR_TABLESPACE_MISSING;
			} else if (dd_space == nullptr) {
				/* not TABLESPACE=innodb_file_per_table */
			} else {
				dict_table_t::filename_t	filename;
				std::string			data_directory;

				new_table->se_private_data().get(
					dd_table_key_strings[
						DD_TABLE_DATA_DIRECTORY],
					data_directory);

				if (ib_table->get_filename(
					    filename, data_directory.empty()
					    ? nullptr : data_directory.c_str(),
					    dict_table_t::SUFFIX_NORMAL)
				    || dd_ibd_rename(client, dd_space,
						     ib_table->first_index()
						     ->space_id(), filename)) {
					error = HA_ERR_WRONG_IN_RECORD;
				}

				delete dd_space;
			}
		}
	}

	if (error == 0 && ib_table->has_persistent_stats()) {
		dberr_t ret = dict_stats_rename_table(trx, old_name, ib_table);
		error = convert_error_code_to_mysql(ret, thd);
	}

	if (!need_open) {
	} else if (uncached) {
		ib_table->destroy();
	} else {
		mutex_enter(&dict_sys->mutex);
		ib_table->release();
		mutex_exit(&dict_sys->mutex);
	}

	DBUG_RETURN(error);
}

/** RENAME a partitioned TABLE.
@param[in]	old_table		table to rename from
@param[in]	new_table		table to rename to */
int
ha_innopart::rename_table_impl(
	const dd::Table*	old_table,
	const dd::Table*	new_table)
{
	DBUG_ENTER("ha_innopart::rename_table_impl");
	ut_ad(old_table->partition_type() != dd::Table::PT_NONE);
	ut_ad(new_table->partition_type() == old_table->partition_type());
	ut_ad(new_table->subpartition_type()
	      == old_table->subpartition_type());
	ut_ad(old_table->se_private_id() == new_table->se_private_id());
	ut_ad(old_table->se_private_data().raw_string()
	      == new_table->se_private_data().raw_string());

	ut_ad(!dict_sys_t::hardcoded(old_table->se_private_id()));

	if (high_level_read_only) {
		my_error(ER_READ_ONLY_MODE, MYF(0));
		DBUG_RETURN(HA_ERR_TABLE_READONLY);
	}

	int		error		= 0;
	THD*		thd		= ha_thd();
	trx_t*		trx		= check_trx_exists(thd);
	bool		uncached	= false;

	trx_start_if_not_started(trx, false);
	innobase_register_trx(ht, thd, trx);

	dd::cache::Dictionary_client* client = dd::get_dd_client(thd);
	dd::cache::Dictionary_client::Auto_releaser releaser(client);
	const dd::Schema* old_schema;
	const dd::Schema* new_schema;
	if (client->acquire<dd::Schema>(old_table->schema_id(),
					&old_schema)
	    || client->acquire<dd::Schema>(new_table->schema_id(),
					   &new_schema)) {
		return(HA_ERR_INTERNAL_ERROR);
	}

	char			old_name_buf[2 * (NAME_LEN + 1)];
	char			new_name_buf[2 * (NAME_LEN + 1)];
	snprintf(old_name_buf, sizeof old_name_buf, "%s%c%s",
		 old_schema->name().c_str(), 0,
		 old_table->name().c_str());
	snprintf(new_name_buf, sizeof new_name_buf, "%s%c%s",
		 new_schema->name().c_str(), 0,
		 new_table->name().c_str());
	const table_name_t	old_name(old_name_buf);
	const table_name_t	new_name(new_name_buf);

	ut_ad(dd::has_exclusive_table_mdl(
		      thd, old_name.db(), old_name.table()));
	ut_ad(dd::has_exclusive_table_mdl(
		      thd, new_name.db(), new_name.table()));

	dict_table_t::filename_t filename;
	std::string		table_data_directory;
	new_table->se_private_data().get(
		dd_table_key_strings[DD_TABLE_DATA_DIRECTORY],
		table_data_directory);

	for (const dd::Partition* dd_part : old_table->partitions()) {
		if (!dd_part_is_stored(dd_part)) {
			continue;
		}
		dict_table_t*		part;

		error = table == nullptr
			? dd_open_table(client, *old_table, dd_part,
					nullptr, &uncached, part)
			: dd_convert_part(client, table,
					  &uncached, part, *dd_part);

		ut_ad((part == nullptr) == (error != 0));
		switch (error) {
		case 0:
			break;
		case HA_ERR_TABLESPACE_MISSING:
			my_error(ER_TABLESPACE_MISSING, MYF(0),
				 old_table->name().c_str());
			/* fall through */
		default:
			DBUG_RETURN(error);
		}

		ut_ad(old_name == part->name);
		ut_ad(dd_has_mdl(part, true));

		if (uncached) {
			part->name = new_name;
		} else if (old_name == new_name) {
			/* No change to table name. */
		} else {
			/* WL#7016 TODO: rename in dict_table_t
			(or rebuild the dict_table_t)
			when the transaction is committed. */
			char*	buf = static_cast<char*>(
				mem_heap_dup(part->heap,
					     new_name.db(),
					     new_name.size()));
			part->name.set_names(buf,
					     buf + new_name.db_size());
		}

		dd::Tablespace* dd_space;

		if (dd_get_implicit_tablespace(
			    client,
			    dd_first_index(dd_part)->tablespace_id(),
			    part->name, dd_space)) {
			error = HA_ERR_TABLESPACE_MISSING;
		} else if (dd_space == nullptr) {
			/* not TABLESPACE=innodb_file_per_table */
		} else {
			std::string	part_data_directory;
			dd_part->options().get("data_file_name",
					       part_data_directory);
			const char*	data_directory
				= !part_data_directory.empty()
				? part_data_directory.c_str()
				: !table_data_directory.empty()
				? table_data_directory.c_str()
				: nullptr;

			if (part->get_filename(filename, data_directory,
					       dict_table_t::SUFFIX_NORMAL)
			    || dd_ibd_rename(client, dd_space,
					     part->first_index()->space_id(),
					     filename)) {
				error = HA_ERR_WRONG_IN_RECORD;
			}

			delete dd_space;
		}

		if (error == 0 && part->has_persistent_stats()) {
			dberr_t	ret	= dict_stats_rename_table(
				trx, old_name, part);
			error = convert_error_code_to_mysql(
				ret, client->thd());
		}

		if (uncached) {
			part->destroy();
		} else {
			mutex_enter(&dict_sys->mutex);
			part->release();
			mutex_exit(&dict_sys->mutex);
		}

		if (error != 0) {
			break;
		}
	}

	DBUG_RETURN(error);
}

/** Initialize the share with table and indexes per partition.
@param[in,out]	thd		thread context
@param[in]	table_share	MySQL table definition
@param[in]	dd_table	Global DD table object
@param[in]	part_info	partition names to use
@return	error code
@retval 0	on success */
int
Ha_innopart_share::open_table_parts(
	THD*			thd,
	const TABLE*		table,
	const dd::Table&	dd_table,
	const partition_info*	part_info)
{
	ut_ad(dd_table.partition_type() != dd::Table::PT_NONE);
#ifndef DBUG_OFF
	if (m_table_share->tmp_table == NO_TMP_TABLE) {
		mysql_mutex_assert_owner(&m_table_share->LOCK_ha_data);
	}
#endif /* DBUG_OFF */
	m_ref_count++;
	if (m_table_parts != nullptr) {
		ut_d(std::string data_directory);
		ut_d(dd_table.se_private_data().get(
			     dd_table_key_strings[DD_TABLE_DATA_DIRECTORY],
			     data_directory));
		ut_ad((get_data_directory() == nullptr)
		      == data_directory.empty());
		ut_ad(data_directory.empty()
		      || data_directory
		      == std::string(get_data_directory(),
				     strlen(get_data_directory()) - 1));
		ut_ad(m_ref_count > 1);
		ut_ad(m_tot_parts > 0);

		/* Increment dict_table_t reference count for all partitions */
		mutex_enter(&dict_sys->mutex):
		for (uint i = 0; i < m_tot_parts; i++) {
			dict_table_t*	table = m_table_parts[i];
			table->acquire();
			ut_ad(table->get_ref_count() >= m_ref_count);
		}
		mutex_exit(&dict_sys->mutex);

		return(0);
	}
	ut_ad(m_ref_count == 1);
	ut_ad(get_data_directory() == nullptr);

	std::string	data_directory;
	dd_table.se_private_data().get(
		dd_table_key_strings[DD_TABLE_DATA_DIRECTORY], data_directory);
	if (!data_directory.empty()) {
		set_data_directory(data_directory);
	}

	m_tot_parts = part_info->get_tot_partitions();
	m_table_parts = static_cast<dict_table_t**>(
		ut_zalloc(m_tot_parts * sizeof *m_table_parts,
			  mem_key_partitioning));
	if (m_table_parts == nullptr) {
		m_ref_count--;
		return(HA_ERR_OUT_OF_MEM);
	}

	dd::cache::Dictionary_client*			client
		= dd::get_dd_client(thd);
	dd::cache::Dictionary_client::Auto_releaser	releaser(client);

	/* Set up the array over all table partitions. */
	uint			i = 0;

	for (const dd::Partition* dd_part : dd_table.partitions()) {
		if (!dd_part_is_stored(dd_part)) {
			continue;
		}

		if (int error = dd_convert_part(
			    client, table, nullptr,
			    m_table_parts[i], *dd_part)) {
			ut_ad(m_table_parts[i] == nullptr);
			close_table_parts(false);
			return(error);
		}

		ut_ad(m_table_parts[i] != nullptr);
		ut_ad(m_table_parts[i]->get_num_indexes()
		      - !m_table_parts[i]->has_primary_key()
		      == part_info->table->s->keys);
		i++;
	}

	ut_ad(i == m_tot_parts);
	return(0);
}

/** Attach a cloned ha_innopart instance. */
inline
void
Ha_innopart_share::clone()
{
#ifndef DBUG_OFF
	mysql_mutex_assert_owner(&m_table_share->LOCK_ha_data);
#endif /* DBUG_OFF */
	ut_ad(!is_closed());
	ut_ad(m_ref_count > 0);
	ut_ad(m_tot_parts > 0);
	ut_ad(m_table_parts != nullptr);
	m_ref_count++;
	mutex_enter(&dict_sys->mutex);
	for (uint i = 0; i < m_tot_parts; i++) {
		dict_table_t*	table = m_table_parts[i];
		table->acquire();
		ut_ad(table->get_ref_count() >= m_ref_count);
	}
	mutex_exit(&dict_sys->mutex);
}

/** Open an InnoDB table.
@param[in]	dd_table	data dictionary table
(NULL if internally created temporary table)
@retval 0 on success
@retval HA_ERR_NO_SUCH_TABLE if the table does not exist */
int
ha_innobase::open(const char*, int, uint, const dd::Table* dd_table)
{
	dict_table_t*		ib_table;
	THD*			thd;

	DBUG_ENTER("ha_innobase::open");
	ut_ad(table_share == table->s);
	ut_ad(dd_table == nullptr
	      || dd_table->partition_type() == dd::Table::PT_NONE);

	thd = ha_thd();

	/* We must not hold an adaptive search latch while acquiring
	higher-ordered latches. */
	innobase_release_temporary_latches(ht, thd);

	m_user_thd = nullptr;

	/* Will be allocated if it is needed in ::update_row() */
	m_upd_buf = nullptr;
	m_upd_buf_size = 0;

	lock_shared_ha_data();
	InnoDB_share*	share = get_share();
	ib_table = share ? share->get_table() : nullptr;
	unlock_shared_ha_data();

	if (dd_table == nullptr) {
		ut_ad(ib_table != nullptr);
		ut_ad(ib_table->is_internal());
		ut_ad(!ib_table->has_autoinc());
		ib_table->acquire();
	} else if (ib_table != nullptr) {
		ut_ad(!ib_table->is_internal());
		ut_d(std::string data_directory);
		ut_d(dd_table->se_private_data().get(
			     dd_table_key_strings[DD_TABLE_DATA_DIRECTORY],
			     data_directory));
		ut_ad((share->get_data_directory() == nullptr)
		      == data_directory.empty());
		ut_ad(data_directory.empty()
		      || data_directory
		      == std::string(share->get_data_directory(),
				     strlen(share->get_data_directory()) - 1));
		mutex_enter(&dict_sys->mutex);
		ib_table->acquire();
		mutex_exit(&dict_sys->mutex);
	} else if (share != nullptr) {
		dd::cache::Dictionary_client*			client
			= dd::get_dd_client(thd);
		dd::cache::Dictionary_client::Auto_releaser	rel(client);

		if (int	err = dd_convert_table(client, table, share,
					       nullptr, ib_table, *dd_table)) {
			set_my_errno(ENOENT);
			DBUG_RETURN(err);
		}
		ut_ad(!dict_table_is_intrinsic(ib_table));
	}

	if (share == NULL) {
		ut_ad(ib_table == nullptr);
		DBUG_RETURN(HA_ERR_OUT_OF_MEM);
	}

	ut_ad(ib_table->stat_initialized);
	ut_ad(!ib_table->has_autoinc() || ib_table->get_autoinc() != 0);

	MONITOR_INC(MONITOR_TABLE_OPEN);

	m_prebuilt = row_create_prebuilt(ib_table, table);

	key_used_on_scan = table_share->primary_key;

	/* Allocate a buffer for a 'row reference'. A row reference is
	a string of bytes of length ref_length which uniquely specifies
	a row in our table. Note that MySQL may also compare two row
	references for equality by doing a simple memcmp on the strings
	of length ref_length! */

	ut_ad((table_share->primary_key == MAX_KEY)
	      == m_prebuilt->clust_index_was_generated);

	ref_length = m_prebuilt->clust_index_was_generated
		? DATA_ROW_ID_LEN
		: table->key_info[table_share->primary_key].key_length;

	/* Index block size in InnoDB: used by MySQL in query optimization */
	stats.block_size = UNIV_PAGE_SIZE;

	info(HA_STATUS_NO_LOCK | HA_STATUS_VARIABLE | HA_STATUS_CONST);
	DBUG_RETURN(0);
}

/** Clone the currently open table handle.
@param[in,out]	mem_root	memory context
@return cloned handler
@retval NULL on failure */
handler*
ha_innobase::clone(const char*, MEM_ROOT* mem_root)
{
	DBUG_ENTER("ha_innobase::clone");
	ut_ad(table->part_info == nullptr);
	ut_ad((table_share->primary_key == MAX_KEY
	       ? DATA_ROW_ID_LEN
	       : table->key_info[table_share->primary_key].key_length)
	      == ref_length);

	ha_innobase*	new_handler = dynamic_cast<ha_innobase*>(
		ha_clone_prepare(mem_root));
	if (new_handler != nullptr) {
		mutex_enter(&dict_sys->mutex);
		ut_ad(m_prebuilt->table->get_ref_count() > 0);
		m_prebuilt->table->acquire();
		mutex_exit(&dict_sys->mutex);
		new_handler->clone_from(*this);
	}
	DBUG_RETURN(new_handler);
}

/** Clone the currently open table handle.
@param[in,out]	mem_root	memory context
@return cloned handler
@retval nullptr on failure */
handler*
ha_innopart::clone(const char*, MEM_ROOT* mem_root)
{
	DBUG_ENTER("ha_innopart::clone");
	ut_ad(m_part_info == table->part_info);
	ut_ad(m_part_info != nullptr);
	ut_ad(m_part_share != nullptr);
	ut_ad(m_tot_parts > 0);
	ut_ad(ref_length >= PARTITION_BYTES_IN_POS);
	ut_ad((table_share->primary_key == MAX_KEY
	       ? DATA_ROW_ID_LEN
	       : table->key_info[table_share->primary_key].key_length)
	      == ref_length - PARTITION_BYTES_IN_POS);
	ut_ad(ha_innopart::m_part_share == Partition_helper::m_part_share);

	ha_innopart*	new_handler = dynamic_cast<ha_innopart*>(
		ha_clone_prepare(mem_root));
	if (new_handler != nullptr) {
		if (new_handler->Partition_helper::clone(*this)) {
			DBUG_RETURN(nullptr);
		}
		new_handler->m_part_share = m_part_share;
		ut_ad(new_handler->ha_innopart::m_part_share
		      == new_handler->Partition_helper::m_part_share);

		new_handler->m_ins_node_parts = static_cast<ins_node_t**>(
			ut_zalloc(m_tot_parts * sizeof *m_ins_node_parts,
				  mem_key_partitioning));

		new_handler->m_upd_node_parts = static_cast<upd_node_t**>(
			ut_zalloc(m_tot_parts * sizeof *m_upd_node_parts,
				  mem_key_partitioning));

		new_handler->alloc_blob_heap_array();

		new_handler->m_version_parts = static_cast<unsigned*>(
			ut_zalloc(m_tot_parts * sizeof *m_version_parts,
				  mem_key_partitioning));

		new_handler->m_row_read_type_parts = static_cast<ulint*>(
			ut_zalloc(m_tot_parts * sizeof *m_row_read_type_parts,
				  mem_key_partitioning));

		new_handler->m_bitset = static_cast<byte*>(
			ut_zalloc(UT_BITS_IN_BYTES(m_tot_parts),
				  mem_key_partitioning));

		new_handler->m_pcur_parts = nullptr;
		new_handler->m_clust_pcur_parts = nullptr;
		new_handler->m_pcur_map = nullptr;
		new_handler->m_pcur = nullptr;
		new_handler->m_clust_pcur = nullptr;

		if (new_handler->m_ins_node_parts == nullptr
		    || new_handler->m_upd_node_parts == nullptr
		    || new_handler->m_blob_heap_parts == nullptr
		    || new_handler->m_version_parts == nullptr
		    || new_handler->m_row_read_type_parts == nullptr
		    || new_handler->m_bitset == nullptr) {
			ut_free(new_handler->m_ins_node_parts);
			ut_free(new_handler->m_upd_node_parts);
			ut_free(new_handler->m_blob_heap_parts);
			ut_free(new_handler->m_version_parts);
			ut_free(new_handler->m_row_read_type_parts);
			ut_free(new_handler->m_bitset);

			DBUG_RETURN(nullptr);
		}

		new_handler->m_sql_stat_start_parts.init(
			new_handler->m_bitset, UT_BITS_IN_BYTES(m_tot_parts));
		memcpy(new_handler->m_version_parts, m_version_parts,
		       m_tot_parts * sizeof *m_version_parts);
		new_handler->clone_from(*this);
		lock_shared_ha_data();
		m_part_share->clone();
		unlock_shared_ha_data();
	}

	DBUG_RETURN(new_handler);
}

/** Implement clone()
@param[in]	from	handle being cloned to this */
inline
void
ha_innobase::clone_from(const ha_innobase& from)
{
	ut_ad(table_share == from.table_share);
	ut_ad(from.table->s == table_share);
	ut_ad(from.m_prebuilt != nullptr);
	ut_ad(from.m_prebuilt->default_rec != nullptr);
	ut_ad(from.m_prebuilt->default_rec
	      == table_share->default_values);
	ut_ad(from.m_prebuilt->table != nullptr);
	ut_ad(!from.m_prebuilt->table->is_internal());
	ut_ad((table_share->primary_key == MAX_KEY)
	      == from.m_prebuilt->clust_index_was_generated);
	ut_ad(from.dup_ref == from.ref + ALIGN_SIZE(from.ref_length));
	ha_open_psi();

	/* Do what ha_innobase::open() would do, but more efficiently,
	because we already have an open table handle. */
	table = from.table;
	key_used_on_scan = table_share->primary_key;
	ref_length = from.ref_length;
	m_user_thd = from.m_user_thd;
	stats = from.stats;
	/* ha_innobase::update_row() will allocate these if needed */
	m_upd_buf = nullptr;
	m_upd_buf_size = 0;

	mem_heap_t*	heap		= mem_heap_create(
		mem_heap_get_size(from.m_prebuilt->heap));
	row_prebuilt_t*	prebuilt	= static_cast<row_prebuilt_t*>(
		mem_heap_dup(heap, from.m_prebuilt, sizeof *prebuilt));
	m_prebuilt = prebuilt;
	prebuilt->heap = heap;

	const unsigned n_cols = prebuilt->table->has_index_on_virtual()
		? prebuilt->table->get_n_user_cols() : 0;
	ut_ad(prebuilt->vcols.n_bits == n_cols);
	ut_ad(prebuilt->bcols.n_bits == n_cols);
	if (n_cols > 0) {
		size_t s = sizeof(my_bitmap_map) * bitmap_buffer_size(n_cols);
		prebuilt->vcols.bitmap = static_cast<my_bitmap_map*>(
			mem_heap_dup(heap, prebuilt->vcols.bitmap, s));
		prebuilt->vcols.last_word_ptr
			+= prebuilt->vcols.bitmap
			- from.m_prebuilt->vcols.bitmap;
		prebuilt->bcols.bitmap = static_cast<my_bitmap_map*>(
			mem_heap_dup(heap, prebuilt->bcols.bitmap, s));
		prebuilt->bcols.last_word_ptr
			+= prebuilt->bcols.bitmap
			- from.m_prebuilt->bcols.bitmap;
	}

	/* The following is adapted from row_create_prebuilt(). */
	ut_ad((prebuilt->srch_key_val1 == nullptr)
	      == (prebuilt->srch_key_val_len == 0));
	ut_ad((prebuilt->srch_key_val2 == nullptr)
	      == (prebuilt->srch_key_val_len == 0));

	if (prebuilt->srch_key_val_len) {
		prebuilt->srch_key_val1 = static_cast<byte*>(
			mem_heap_alloc(prebuilt->heap,
				       2 * prebuilt->srch_key_val_len));
		prebuilt->srch_key_val2 = prebuilt->srch_key_val1 +
			prebuilt->srch_key_val_len;
	}

	prebuilt->pcur = static_cast<btr_pcur_t*>(
		mem_heap_zalloc(heap, sizeof *prebuilt->pcur));
	prebuilt->clust_pcur = static_cast<btr_pcur_t*>(
		mem_heap_zalloc(heap, sizeof *prebuilt->clust_pcur));
	btr_pcur_reset(prebuilt->pcur);
	btr_pcur_reset(prebuilt->clust_pcur);

	const dict_index_t*	clust_index
		= prebuilt->table->first_index();
	const unsigned		ref_len	= clust_index->n_uniq;

	prebuilt->search_tuple = dtuple_create(
		heap, 2 * prebuilt->table->get_n_cols());
	prebuilt->clust_ref = dtuple_create(heap, ref_len);
	clust_index->copy_types(prebuilt->clust_ref, ref_len);

	/* row_create_prebuilt() is using mem_heap_zalloc(), so it
	zero-initialized all the pointer fields.  Let us do the same,
	so that m_prebuilt will not be wrongly sharing
	pointers with from.m_prebuilt. */
	prebuilt->blob_heap = nullptr;
	prebuilt->old_vers_heap = nullptr;
	prebuilt->mysql_template = nullptr;
	prebuilt->ins_node = nullptr;
	prebuilt->ins_upd_rec_buff = nullptr;
	prebuilt->upd_node = nullptr;
	prebuilt->ins_graph = nullptr;
	prebuilt->upd_graph = nullptr;
	prebuilt->sel_graph = nullptr;
	memset(prebuilt->fetch_cache, 0, sizeof *prebuilt->fetch_cache);
	prebuilt->rtr_info = nullptr;

	reset_template();

	MONITOR_INC(MONITOR_TABLE_OPEN);
}
