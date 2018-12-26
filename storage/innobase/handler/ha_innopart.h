/*****************************************************************************

Copyright (c) 2014, 2017, Oracle and/or its affiliates. All Rights Reserved.

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

/* The InnoDB Partition handler: the interface between MySQL and InnoDB. */

#ifndef ha_innopart_h
#define ha_innopart_h

#include "partitioning/partition_handler.h"

/* Forward declarations */
class Altered_partitions;
class partition_info;

/** HA_DUPLICATE_POS and HA_READ_BEFORE_WRITE_REMOVAL is not
set from ha_innobase, but cannot yet be supported in ha_innopart.
Full text and geometry is not yet supported. */
const handler::Table_flags	HA_INNOPART_DISABLED_TABLE_FLAGS =
	( HA_CAN_FULLTEXT
	| HA_CAN_FULLTEXT_EXT
	| HA_CAN_GEOMETRY
	| HA_DUPLICATE_POS
	| HA_READ_BEFORE_WRITE_REMOVAL);

/** InnoDB partition specific Handler_share. */
class Ha_innopart_share : public Partition_share
{
private:
	/** Array of all included table definitions (one per partition). */
	dict_table_t**		m_table_parts;

	/** Instead of INNOBASE_SHARE::idx_trans_tbl. Maps MySQL index number
	to InnoDB index per partition. */
	dict_index_t**		m_index_mapping;

	/** Total number of partitions. */
	uint			m_tot_parts;

	/** Number of indexes. */
	uint			m_index_count;

	/** Reference count. */
	uint			m_ref_count;

	/** Pointer back to owning TABLE_SHARE. */
	TABLE_SHARE*		m_table_share;

public:
	Ha_innopart_share(
		TABLE_SHARE*	table_share);

	~Ha_innopart_share();

	/** Set innodb table for given partition.
	@param[in]	part_id	Partition number.
	@param[in]	table	Table. */
	inline
	void
	set_table_part(
		uint		part_id,
		dict_table_t*	table)
	{
		ut_ad(m_table_parts != NULL);
		ut_ad(part_id < m_tot_parts);
		m_table_parts[part_id] = table;
	}

	/** Return innodb table for given partition.
	@param[in]	part_id	Partition number.
	@return	InnoDB table. */
	inline
	dict_table_t*
	get_table_part(
		uint	part_id) const
	{
		ut_ad(m_table_parts != NULL);
		ut_ad(part_id < m_tot_parts);
		return(m_table_parts[part_id]);
	}

	/** Return innodb index for given partition and key number.
	@param[in]	part_id	Partition number.
	@param[in]	keynr	Key number.
	@return	InnoDB index. */
	dict_index_t*
	get_index(
		uint	part_id,
		uint	keynr);

	/** Get MySQL key number corresponding to InnoDB index.
	@param[in]	part_id	Partition number.
	@param[in]	index	InnoDB index.
	@return	MySQL key number or MAX_KEY if non-existent. */
	uint
	get_mysql_key(
		uint			part_id,
		const dict_index_t*	index);

	/** Initialize the share with table and indexes per partition.
	@param[in]	part_info	Partition info (partition names to use)
	@param[in]	table_name	Table name (db/table_name)
	@return false on success else true. */
	bool
	open_table_parts(
		partition_info*	part_info,
		const char*	table_name);

	/** Close the table partitions.
	If all instances are closed, also release the resources. */
	void
	close_table_parts();

	/* Static helper functions. */
	/** Fold to lower case if windows or lower_case_table_names == 1.
	@param[in,out]	s	String to fold.*/
	static
	void
	partition_name_casedn_str(
		char*	s);

	/** Translate and append partition name.
	@param[out]	to	String to write in filesystem charset
	@param[in]	from	Name in system charset
	@param[in]	sep	Separator
	@param[in]	len	Max length of to buffer
	@return	length of written string. */
	static
	size_t
	append_sep_and_name(
		char*		to,
		const char*	from,
		const char*	sep,
		size_t		len);

	/** Set up the virtual column template for partition table, and points
	all m_table_parts[]->vc_templ to it.
	@param[in]      table           MySQL TABLE object
	@param[in]      ib_table        InnoDB dict_table_t
	@param[in]      table_name      Table name (db/table_name) */
	void
	set_v_templ(
		TABLE*		table,
		dict_table_t*	ib_table,
		const char*	name);

private:
	/** Disable default constructor. */
	Ha_innopart_share() {};

	/** Open one partition (lower lever innodb table).
	@param[in]	part_id	Partition to open.
	@param[in]	partition_name	Name of partition.
	@return false on success else true. */
	bool
	open_one_table_part(
		uint		part_id,
		const char*	partition_name);
};

/** The class defining a partitioning aware handle to an InnoDB table.
Based on ha_innobase and extended with
- Partition_helper for re-using common partitioning functionality
- Partition_handler for providing partitioning specific api calls.
Generic partitioning functions are implemented in Partition_helper.
Lower level storage functions are implemented in ha_innobase.
Partition_handler is inherited for implementing the handler level interface
for partitioning specific functions, like change_partitions and
truncate_partition.
InnoDB specific functions related to partitioning is implemented here. */
class ha_innopart:
	public ha_innobase,
	public Partition_helper,
	public Partition_handler
{
public:
	ha_innopart(
		handlerton*	hton,
		TABLE_SHARE*	table_arg);

	~ha_innopart();

	/** Clone this handler, used when needing more than one cursor
	to the same table.
	@param[in]	name		Table name.
	@param[in]	mem_root	mem_root to allocate from.
	@retval	Pointer to clone or NULL if error. */
	handler*
	clone(
		const char*	name,
		MEM_ROOT*	mem_root);

	/** Check and register a table in the query cache.
	Ask InnoDB if a query to a table can be cached.
	@param[in]	thd		User thread handle.
	@param[in]	table_key	Normalized path to the table.
	@param[in]	key_length	Lenght of table_key.
	@param[out]	call_back	Function pointer for checking if data
	has changed.
	@param[in,out]	engine_data	Data for call_back (not used).
	@return TRUE if query caching of the table is permitted. */
	my_bool
	register_query_cache_table(
		THD*			thd,
		char*			table_key,
		size_t			key_length,
		qc_engine_callback*	call_back,
		ulonglong*		engine_data)
	{
		/* Currently this would need to go through every
		[sub] partition in the table to see if any of them has changed.
		See row_search_check_if_query_cache_permitted().
		So disabled until we can avoid check all partitions. */
		return(FALSE);
	}

	/** On-line ALTER TABLE interface @see handler0alter.cc @{ */

	/** Check if InnoDB supports a particular alter table in-place.
	@param[in]	altered_table	TABLE object for new version of table.
	@param[in,out]	ha_alter_info	Structure describing changes to be done
	by ALTER TABLE and holding data used during in-place alter.
	@retval	HA_ALTER_INPLACE_NOT_SUPPORTED	Not supported
	@retval	HA_ALTER_INPLACE_NO_LOCK	Supported
	@retval	HA_ALTER_INPLACE_SHARED_LOCK_AFTER_PREPARE	Supported, but
	requires lock during main phase and exclusive lock during prepare
	phase.
	@retval	HA_ALTER_INPLACE_NO_LOCK_AFTER_PREPARE	Supported, prepare
	phase requires exclusive lock. */
	enum_alter_inplace_result
	check_if_supported_inplace_alter(
		TABLE*			altered_table,
		Alter_inplace_info*	ha_alter_info);

	/** Prepare in-place ALTER for table.
	Allows InnoDB to update internal structures with concurrent
	writes blocked (provided that check_if_supported_inplace_alter()
	did not return HA_ALTER_INPLACE_NO_LOCK).
	This will be invoked before inplace_alter_table().
	@param[in]	altered_table	TABLE object for new version of table.
	@param[in,out]	ha_alter_info	Structure describing changes to be done
	by ALTER TABLE and holding data used during in-place alter.
	@retval	true	Failure.
	@retval	false	Success. */
	bool
	prepare_inplace_alter_table(
		TABLE*			altered_table,
		Alter_inplace_info*	ha_alter_info);

	/** Alter the table structure in-place.
	Alter the table structure in-place with operations
	specified using HA_ALTER_FLAGS and Alter_inplace_information.
	The level of concurrency allowed during this operation depends
	on the return value from check_if_supported_inplace_alter().
	@param[in]	altered_table	TABLE object for new version of table.
	@param[in,out]	ha_alter_info	Structure describing changes to be done
	by ALTER TABLE and holding data used during in-place alter.
	@retval	true	Failure.
	@retval	false	Success. */
	bool
	inplace_alter_table(
		TABLE*			altered_table,
		Alter_inplace_info*	ha_alter_info);

	/** Commit or rollback.
	Commit or rollback the changes made during
	prepare_inplace_alter_table() and inplace_alter_table() inside
	the storage engine. Note that the allowed level of concurrency
	during this operation will be the same as for
	inplace_alter_table() and thus might be higher than during
	prepare_inplace_alter_table(). (E.g concurrent writes were
	blocked during prepare, but might not be during commit).
	@param[in]	altered_table	TABLE object for new version of table.
	@param[in]	ha_alter_info	Structure describing changes to be done
	by ALTER TABLE and holding data used during in-place alter.
	@param[in,out]	commit		true => Commit, false => Rollback.
	@retval	true	Failure.
	@retval	false	Success. */
	bool
	commit_inplace_alter_table(
		TABLE*			altered_table,
		Alter_inplace_info*	ha_alter_info,
		bool			commit);

	/** Notify the storage engine that the table structure (.frm) has
	been updated.

	ha_partition allows inplace operations that also upgrades the engine
	if it supports partitioning natively. So if this is the case then
	we will remove the .par file since it is not used with ha_innopart
	(we use the internal data dictionary instead). */
	void
	notify_table_changed();
	/** @} */

	// TODO: should we implement init_table_handle_for_HANDLER() ?
	// (or is sql_stat_start handled correctly anyway?)
	int
	optimize(
		THD*		thd,
		HA_CHECK_OPT*	check_opt);

	int
	discard_or_import_tablespace(
		my_bool	discard);

	/** Compare key and rowid.
	Helper function for sorting records in the priority queue.
	a/b points to table->record[0] rows which must have the
	key fields set. The bytes before a and b store the rowid.
	This is used for comparing/sorting rows first according to
	KEY and if same KEY, by rowid (ref).

	@param[in]	key_info	Null terminated array of index
	information.
	@param[in]	a		Pointer to record+ref in first record.
	@param[in]	b		Pointer to record+ref in second record.
	@return Return value is SIGN(first_rec - second_rec)
	@retval	0	Keys are equal.
	@retval	-1	second_rec is greater than first_rec.
	@retval	+1	first_rec is greater than second_rec. */
	static
	int
	key_and_rowid_cmp(
		KEY**	key_info,
		uchar	*a,
		uchar	*b);

	int
	extra(
		enum ha_extra_function	operation);

	void
	print_error(
		int	error,
		myf	errflag);

	bool
	is_ignorable_error(
		int	error);

	int
	start_stmt(
		THD*		thd,
		thr_lock_type	lock_type);

	ha_rows
	records_in_range(
		uint		inx,
		key_range*	min_key,
		key_range*	max_key);

	ha_rows
	estimate_rows_upper_bound();

	uint
	alter_table_flags(
		uint	flags);

	void
	update_create_info(
		HA_CREATE_INFO*	create_info);

	int
	create(
		const char*	name,
		TABLE*		form,
		HA_CREATE_INFO*	create_info);

	int
	truncate();

	int
	check(
		THD*		thd,
		HA_CHECK_OPT*	check_opt);

	/** Repair table.
	Will only handle records in wrong partition, not repairing
	corrupt innodb indexes.
	@param[in]	thd	Thread context.
	@param[in]	repair_opt	Repair options.
	@return 0 or error code. */
	int
	repair(
		THD*		thd,
		HA_CHECK_OPT*	repair_opt);

	bool
	can_switch_engines();

	uint
	referenced_by_foreign_key();

	void
	get_auto_increment(
		ulonglong	offset,
		ulonglong	increment,
		ulonglong	nb_desired_values,
		ulonglong*	first_value,
		ulonglong*	nb_reserved_values);

	int
	cmp_ref(
		const uchar*	ref1,
		const uchar*	ref2);

	int
	read_range_first(
		const key_range*	start_key,
		const key_range*	end_key,
		bool			eq_range_arg,
		bool			sorted)
	{
		return(Partition_helper::ph_read_range_first(
						start_key,
						end_key,
						eq_range_arg,
						sorted));
	}

	void
	position(
		const uchar*	record)
	{
		Partition_helper::ph_position(record);
	}

	/* TODO: Implement these! */
	bool
	check_if_incompatible_data(
		HA_CREATE_INFO*	info,
		uint		table_changes)
	{
		ut_ad(0);
		return(COMPATIBLE_DATA_NO);
	}

	int
	delete_all_rows()
	{
		return(handler::delete_all_rows());
	}

	int
	disable_indexes(
		uint	mode)
	{
		return(HA_ERR_WRONG_COMMAND);
	}

	int
	enable_indexes(
		uint	mode)
	{
		return(HA_ERR_WRONG_COMMAND);
	}

	void
	free_foreign_key_create_info(
		char*	str)
	{
		ut_ad(0);
	}

	int
	ft_init()
	{
		ut_ad(0);
		return(HA_ERR_WRONG_COMMAND);
	}

	FT_INFO*
	ft_init_ext(
		uint	flags,
		uint	inx,
		String*	key)
	{
		ut_ad(0);
		return(NULL);
	}

	FT_INFO*
	ft_init_ext_with_hints(
		uint		inx,
		String*		key,
		Ft_hints*	hints)
	{
		ut_ad(0);
		return(NULL);
	}

	int
	ft_read(
		uchar*	buf)
	{
		ut_ad(0);
		return(HA_ERR_WRONG_COMMAND);
	}

	bool
	get_foreign_dup_key(
		char*	child_table_name,
		uint	child_table_name_len,
		char*	child_key_name,
		uint	child_key_name_len)
	{
		ut_ad(0);
		return(false);
	}

	// TODO: not yet supporting FK.
	char*
	get_foreign_key_create_info()
	{
		return(NULL);
	}

	// TODO: not yet supporting FK.
	int
	get_foreign_key_list(
		THD*			thd,
		List<FOREIGN_KEY_INFO>*	f_key_list)
	{
		return(0);
	}

	// TODO: not yet supporting FK.
	int
	get_parent_foreign_key_list(
		THD*			thd,
		List<FOREIGN_KEY_INFO>*	f_key_list)
	{
		return(0);
	}

	// TODO: not yet supporting FK.
	int
	get_cascade_foreign_key_table_list(
		THD*				thd,
		List<st_handler_tablename>*	fk_table_list)
	{
		return(0);
	}

	int
	read_range_next()
	{
		return(Partition_helper::ph_read_range_next());
	}

	uint32
	calculate_key_hash_value(
		Field**	field_array)
	{
		return(Partition_helper::ph_calculate_key_hash_value(field_array));
	}

	Table_flags
	table_flags() const
	{
		return(ha_innobase::table_flags() | HA_CAN_REPAIR);
	}

	void
	release_auto_increment()
	{
		Partition_helper::ph_release_auto_increment();
	}

	/** Implementing Partition_handler interface @see partition_handler.h
	@{ */

	/** See Partition_handler. */
	void
	get_dynamic_partition_info(
		ha_statistics*	stat_info,
		ha_checksum*	check_sum,
		uint		part_id)
	{
		Partition_helper::get_dynamic_partition_info_low(
			stat_info,
			check_sum,
			part_id);
	}

	uint
	alter_flags(
		uint	flags MY_ATTRIBUTE((unused))) const
	{
		return(HA_PARTITION_FUNCTION_SUPPORTED
		       | HA_FAST_CHANGE_PARTITION);
	}

	Partition_handler*
	get_partition_handler()
	{
		return(static_cast<Partition_handler*>(this));
	}

	void
	set_part_info(
		partition_info*	part_info,
		bool		early)
	{
		Partition_helper::set_part_info_low(part_info, early);
	}

	void
	initialize_partitioning(
		partition_info*	part_info,
		bool		early)
	{
		Partition_helper::set_part_info_low(part_info, early);
	}

	handler*
	get_handler()
	{
		return(static_cast<handler*>(this));
	}
	/** @} */

private:
	/** Pointer to Ha_innopart_share on the TABLE_SHARE. */
	Ha_innopart_share*	m_part_share;

	/** ins_node per partition. Synchronized with prebuilt->ins_node
	when changing partitions. */
	ins_node_t**		m_ins_node_parts;

	/** upd_node per partition. Synchronized with prebuilt->upd_node
	when changing partitions. */
	upd_node_t**		m_upd_node_parts;

	/** blob_heap per partition. Synchronized with prebuilt->blob_heap
	when changing partitions. */
	mem_heap_t**		m_blob_heap_parts;

	/** trx_id from the partitions table->def_trx_id. Keep in sync
	with prebuilt->trx_id when changing partitions.
	prebuilt only reflects the current partition! */
	trx_id_t*		m_trx_id_parts;

	/** row_read_type per partition. */
	ulint*			m_row_read_type_parts;

	/** sql_stat_start per partition. */
	uchar*			m_sql_stat_start_parts;

	/** persistent cursors per partition. */
	btr_pcur_t*		m_pcur_parts;

	/** persistent cluster cursors per partition. */
	btr_pcur_t*		m_clust_pcur_parts;

	/** map from part_id to offset in above two arrays. */
	uint16_t*		m_pcur_map;

	/** Original m_prebuilt->pcur. */
	btr_pcur_t*		m_pcur;

	/** Original m_prebuilt->clust_pcur. */
	btr_pcur_t*		m_clust_pcur;

	/** New partitions during ADD/REORG/... PARTITION. */
	Altered_partitions*	m_new_partitions;

	/** Clear used ins_nodes and upd_nodes. */
	void
	clear_ins_upd_nodes();

	/** Clear the blob heaps for all partitions */
	void
	clear_blob_heaps();

	/** Reset state of file to after 'open'. This function is called
	after every statement for all tables used by that statement. */
	int
	reset();

	/** Allocate the array to hold blob heaps for all partitions */
	mem_heap_t**
	alloc_blob_heap_array();

	/** Free the array that holds blob heaps for all partitions */
	void
	free_blob_heap_array();

	/** Changes the active index of a handle.
	@param[in]	part_id	Use this partition.
	@param[in]	keynr	Use this index; MAX_KEY means always
	clustered index, even if it was internally generated by InnoDB.
	@return 0 or error code. */
	int
	change_active_index(
		uint	part_id,
		uint	keynr);

	/** Move to next partition and set its index.
	@return	0 for success else error number. */
	int
	next_partition_index();

	/** Internally called for initializing auto increment value.
	Should never be called, but defined to catch such errors.
	@return 0 on success else error code. */
	int
	innobase_initialize_autoinc();

	/** Get the index for the current partition
	@param[in]	keynr	MySQL index number.
	@return InnoDB index or NULL. */
	dict_index_t*
	innobase_get_index(
		uint	keynr);

	/** Get the index for a handle.
	Does not change active index.
	@param[in]	keynr	use this index; MAX_KEY means always clustered
	index, even if it was internally generated by InnoDB.
	@param[in]	part_id	From this partition.
	@return NULL or index instance. */
	dict_index_t*
	innopart_get_index(
		uint	part_id,
		uint	keynr);

	/** Change active partition.
	Copies needed info into m_prebuilt from the partition specific memory.
	@param[in]	part_id	Partition to set as active. */
	void
	set_partition(
		uint	part_id);

	/** Update active partition.
	Copies needed info from m_prebuilt into the partition specific memory.
	@param[in]	part_id	Partition to set as active. */
	void
	update_partition(
		uint	part_id);

	/** Helpers needed by Partition_helper, @see partition_handler.h @{ */

	/** Set the autoinc column max value.
	This should only be called once from ha_innobase::open().
	Therefore there's no need for a covering lock.
	@param[in]	no_lock	If locking should be skipped. Not used!
	@return 0 on success else error code. */
	int
	initialize_auto_increment(
		bool	/* no_lock */);

	/** Save currently highest auto increment value.
	@param[in]	nr	Auto increment value to save. */
	void
	save_auto_increment(
		ulonglong	nr);

	/** Setup the ordered record buffer and the priority queue.
	@param[in]	used_parts	Number of used partitions in query.
	@return false for success, else true. */
	int
	init_record_priority_queue_for_parts(
		uint	used_parts);

	/** Destroy the ordered record buffer and the priority queue. */
	void
	destroy_record_priority_queue_for_parts();

	/** Prepare for creating new partitions during ALTER TABLE ...
	PARTITION.
	@param[in]	num_partitions	Number of new partitions to be created.
	@param[in]	only_create	True if only creating the partition
	(no open/lock is needed).
	@return 0 for success else error code. */
	int
	prepare_for_new_partitions(
		uint	num_partitions,
		bool	only_create);

	/** Create a new partition to be filled during ALTER TABLE ...
	PARTITION.
	@param[in]	table		Table to create the partition in.
	@param[in]	create_info	Table/partition specific create info.
	@param[in]	part_name	Partition name.
	@param[in]	new_part_id	Partition id in new table.
	@param[in]	part_elem	Partition element.
	@return 0 for success else error code. */
	int
	create_new_partition(
		TABLE*			table,
		HA_CREATE_INFO*		create_info,
		const char*		part_name,
		uint			new_part_id,
		partition_element*	part_elem);

	/** Close and finalize new partitions. */
	void
	close_new_partitions();

	/** write row to new partition.
	@param[in]	new_part	New partition to write to.
	@return 0 for success else error code. */
	int
	write_row_in_new_part(
		uint	new_part);

	/** Write a row in specific partition.
	Stores a row in an InnoDB database, to the table specified in this
	handle.
	@param[in]	part_id	Partition to write to.
	@param[in]	row	A row in MySQL format.
	@return error code. */
	int
	write_row_in_part(
		uint	part_id,
		uchar*	row);

	/** Update a row in partition.
	Updates a row given as a parameter to a new value.
	@param[in]	part_id	Partition to update row in.
	@param[in]	old_row	Old row in MySQL format.
	@param[in]	new_row	New row in MySQL format.
	@return error number or 0. */
	int
	update_row_in_part(
		uint		part_id,
		const uchar*	old_row,
		uchar*		new_row);

	/** Deletes a row in partition.
	@param[in]	part_id	Partition to delete from.
	@param[in]	row	Row to delete in MySQL format.
	@return error number or 0. */
	int
	delete_row_in_part(
		uint		part_id,
		const uchar*	row);

	/** Return first record in index from a partition.
	@param[in]	part	Partition to read from.
	@param[out]	record	First record in index in the partition.
	@return error number or 0. */
	int
	index_first_in_part(
		uint	part,
		uchar*	record);

	/** Return last record in index from a partition.
	@param[in]	part	Partition to read from.
	@param[out]	record	Last record in index in the partition.
	@return error number or 0. */
	int
	index_last_in_part(
		uint	part,
		uchar*	record);

	/** Return previous record in index from a partition.
	@param[in]	part	Partition to read from.
	@param[out]	record	Last record in index in the partition.
	@return error number or 0. */
	int
	index_prev_in_part(
		uint	part,
		uchar*	record);

	/** Return next record in index from a partition.
	@param[in]	part	Partition to read from.
	@param[out]	record	Last record in index in the partition.
	@return error number or 0. */
	int
	index_next_in_part(
		uint	part,
		uchar*	record);

	/** Return next same record in index from a partition.
	This routine is used to read the next record, but only if the key is
	the same as supplied in the call.
	@param[in]	part	Partition to read from.
	@param[out]	record	Last record in index in the partition.
	@param[in]	key	Key to match.
	@param[in]	length	Length of key.
	@return error number or 0. */
	int
	index_next_same_in_part(
		uint		part,
		uchar*		record,
		const uchar*	key,
		uint		length);

	/** Start index scan and return first record from a partition.
	This routine starts an index scan using a start key. The calling
	function will check the end key on its own.
	@param[in]	part	Partition to read from.
	@param[out]	record	First matching record in index in the partition.
	@param[in]	key	Key to match.
	@param[in]	keypart_map	Which part of the key to use.
	@param[in]	find_flag	Key condition/direction to use.
	@return error number or 0. */
	int
	index_read_map_in_part(
		uint			part,
		uchar*			record,
		const uchar*		key,
		key_part_map		keypart_map,
		enum ha_rkey_function	find_flag);

	/** Return last matching record in index from a partition.
	@param[in]	part	Partition to read from.
	@param[out]	record	Last matching record in index in the partition.
	@param[in]	key	Key to match.
	@param[in]	keypart_map	Which part of the key to use.
	@return error number or 0. */
	int
	index_read_last_map_in_part(
		uint		part,
		uchar*		record,
		const uchar*	key,
		key_part_map	keypart_map);

	/** Start index scan and return first record from a partition.
	This routine starts an index scan using a start and end key.
	@param[in]	part	Partition to read from.
	@param[out]	record	First matching record in index in the partition.
	if NULL use table->record[0] as return buffer.
	@param[in]	start_key	Start key to match.
	@param[in]	end_key	End key to match.
	@param[in]	eq_range	Is equal range, start_key == end_key.
	@param[in]	sorted	Return rows in sorted order.
	@return error number or 0. */
	int
	read_range_first_in_part(
		uint			part,
		uchar*			record,
		const key_range*	start_key,
		const key_range*	end_key,
		bool			eq_range,
		bool			sorted);

	/** Return next record in index range scan from a partition.
	@param[in]	part	Partition to read from.
	@param[out]	record	First matching record in index in the partition.
	if NULL use table->record[0] as return buffer.
	@return error number or 0. */
	int
	read_range_next_in_part(
		uint	part,
		uchar*	record);

	/** Start index scan and return first record from a partition.
	This routine starts an index scan using a start key. The calling
	function will check the end key on its own.
	@param[in]	part	Partition to read from.
	@param[out]	record	First matching record in index in the partition.
	@param[in]	index	Index to read from.
	@param[in]	key	Key to match.
	@param[in]	keypart_map	Which part of the key to use.
	@param[in]	find_flag	Key condition/direction to use.
	@return error number or 0. */
	int
	index_read_idx_map_in_part(
		uint			part,
		uchar*			record,
		uint			index,
		const uchar*		key,
		key_part_map		keypart_map,
		enum ha_rkey_function	find_flag);

	/** Initialize random read/scan of a specific partition.
	@param[in]	part_id		Partition to initialize.
	@param[in]	table_scan	True for scan else random access.
	@return error number or 0. */
	int
	rnd_init_in_part(
		uint	part_id,
		bool	table_scan);

	/** Get next row during scan of a specific partition.
	@param[in]	part_id	Partition to read from.
	@param[out]	record	Next row.
	@return error number or 0. */
	int
	rnd_next_in_part(
		uint	part_id,
		uchar*	record);

	/** End random read/scan of a specific partition.
	@param[in]	part_id		Partition to end random read/scan.
	@param[in]	table_scan	True for scan else random access.
	@return error number or 0. */
	int
	rnd_end_in_part(
		uint	part_id,
		bool	table_scan);

	/** Get a reference to the current cursor position in the last used
	partition.
	@param[out]	ref	Reference (PK if exists else row_id).
	@param[in]	record	Record to position. */
	void
	position_in_last_part(
		uchar*		ref,
		const uchar*	record);

	/** Read row using position using given record to find.
	Only useful when position is based on primary key
	@param[in]	record  Current record in MySQL Row Format.
	@return error number or 0. */
	int
	rnd_pos_by_record(
		uchar*  record);

        /** Copy a cached MySQL record.
	@param[out]	to_record	Where to copy the MySQL record.
	@param[in]	from_record	Which record to copy. */
	void
	copy_cached_row(
		uchar*		to_record,
		const uchar*	from_record);
	/** @} */

	/* Private handler:: functions specific for native InnoDB partitioning.
	@see handler.h @{ */

	int
	open(
		const char*	name,
		int		mode,
		uint		test_if_locked);

	int
	close();

	double
	scan_time();

	/** Was the last returned row semi consistent read.
	In an UPDATE or DELETE, if the row under the cursor was locked by
	another transaction, and the engine used an optimistic read of the last
	committed row value under the cursor, then the engine returns 1 from
	this function. MySQL must NOT try to update this optimistic value. If
	the optimistic value does not match the WHERE condition, MySQL can
	decide to skip over this row. This can be used to avoid unnecessary
	lock waits.

	If this method returns true, it will also signal the storage
	engine that the next read will be a locking re-read of the row.
	@see handler.h and row0mysql.h
	@return	true if last read was semi consistent else false. */
	bool was_semi_consistent_read();

	/** Try semi consistent read.
	Tell the engine whether it should avoid unnecessary lock waits.
	If yes, in an UPDATE or DELETE, if the row under the cursor was locked
	by another transaction, the engine may try an optimistic read of
	the last committed row value under the cursor.
	@see handler.h and row0mysql.h
	@param[in]	yes	Should semi-consistent read be used. */
	void try_semi_consistent_read(
		bool	yes);

	/** Removes a lock on a row.
	Removes a new lock set on a row, if it was not read optimistically.
	This can be called after a row has been read in the processing of
	an UPDATE or a DELETE query. @see ha_innobase::unlock_row(). */
	void unlock_row();

	int
	index_init(
		uint	index,
		bool	sorted);

	int
	index_end();

	int
	rnd_init(
		bool	scan)
	{
		return(Partition_helper::ph_rnd_init(scan));
	}

	int
	rnd_end()
	{
		return(Partition_helper::ph_rnd_end());
	}

	int
	external_lock(
		THD*	thd,
		int	lock_type);

	THR_LOCK_DATA**
	store_lock(
		THD*			thd,
		THR_LOCK_DATA**		to,
		thr_lock_type		lock_type);

	int
	write_row(
		uchar*	record)
	{
		return(Partition_helper::ph_write_row(record));
	}

	int
	update_row(
		const uchar*	old_record,
		uchar*		new_record)
	{
		return(Partition_helper::ph_update_row(old_record, new_record));
	}

	int
	delete_row(
		const uchar*	record)
	{
		return(Partition_helper::ph_delete_row(record));
	}
	/** @} */

	/** Truncate partition.
	Called from Partition_handler::trunctate_partition(). */
	int
	truncate_partition_low();

	/** Change partitions according to ALTER TABLE ... PARTITION ...
	Called from Partition_handler::change_partitions().
	@param[in]	create_info	Table create info.
	@param[in]	path		Path including db/table_name.
	@param[out]	copied		Number of copied rows.
	@param[out]	deleted		Number of deleted rows.
	@return	0 for success or error code. */
	int
	change_partitions_low(
		HA_CREATE_INFO*		create_info,
		const char*		path,
		ulonglong* const	copied,
		ulonglong* const	deleted)
	{
		return(Partition_helper::change_partitions(
						create_info,
						path,
						copied,
						deleted));
	}

	/** Access methods to protected areas in handler to avoid adding
	friend class Partition_helper in class handler.
	@see partition_handler.h @{ */

	THD*
	get_thd() const
	{
		return ha_thd();
	}

	TABLE*
	get_table() const
	{
		return table;
	}

	bool
	get_eq_range() const
	{
		return eq_range;
	}

	void
	set_eq_range(bool eq_range_arg)
	{
		eq_range= eq_range_arg;
	}

	void
	set_range_key_part(KEY_PART_INFO *key_part)
	{
		range_key_part= key_part;
	}
	/** @} */

	/** Fill in data_dir_path and tablespace name from internal data
	dictionary.
	@param	part_elem	Partition element to fill.
	@param	ib_table	InnoDB table to copy from. */
	void
	update_part_elem(
		partition_element*	part_elem,
		dict_table_t*		ib_table);
protected:
	/* Protected handler:: functions specific for native InnoDB partitioning.
	@see handler.h @{ */

	int
	rnd_next(
		uchar*	record)
	{
		return(Partition_helper::ph_rnd_next(record));
	}

	int
	rnd_pos(
		uchar*	record,
		uchar*	pos);

#ifdef WL6742
	/* Removing WL6742 as part of Bug 23046302 */
	int
	records(
		ha_rows*	num_rows);
#endif

	int
	index_next(
		uchar*	record)
	{
		return(Partition_helper::ph_index_next(record));
	}

	int
	index_next_same(
		uchar*		record,
		const uchar*	key,
		uint		keylen)
	{
		return(Partition_helper::ph_index_next_same(record, key, keylen));
	}

	int
	index_prev(
		uchar*	record)
	{
		return(Partition_helper::ph_index_prev(record));
	}

	int
	index_first(
		uchar*	record)
	{
		return(Partition_helper::ph_index_first(record));
	}

	int
	index_last(
		uchar*	record)
	{
		return(Partition_helper::ph_index_last(record));
	}

	int
	index_read_last_map(
		uchar*		record,
		const uchar*	key,
		key_part_map	keypart_map)
	{
		return(Partition_helper::ph_index_read_last_map(
						record,
						key,
						keypart_map));
	}

	int
	index_read_map(
		uchar*			buf,
		const uchar*		key,
		key_part_map		keypart_map,
		enum ha_rkey_function	find_flag)
	{
		return(Partition_helper::ph_index_read_map(
				buf,
				key,
				keypart_map,
				find_flag));
	}

	int
	index_read_idx_map(
		uchar*			buf,
		uint			index,
		const uchar*		key,
		key_part_map		keypart_map,
		enum ha_rkey_function	find_flag)
	{
		return(Partition_helper::ph_index_read_idx_map(
				buf,
				index,
				key,
				keypart_map,
				find_flag));
	}
	/** @} */

	/** Updates and return statistics.
	Returns statistics information of the table to the MySQL interpreter,
	in various fields of the handle object.
	@param[in]	flag		Flags for what to update and return.
	@param[in]	is_analyze	True if called from ::analyze().
	@return	HA_ERR_* error code or 0. */
	int
	info_low(
		uint	flag,
		bool	is_analyze);
};
#endif /* ha_innopart_h */
