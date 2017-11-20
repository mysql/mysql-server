/*****************************************************************************

Copyright (c) 2017, Oracle and/or its affiliates. All Rights Reserved.

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

#include <algorithm>

#include <sql_class.h>
#include <sql_show.h>
#include <sql_table.h>
#include <sql_tablespace.h>
#include <current_thd.h>
#include "dict0boot.h"
#include "dict0crea.h"
#include "dict0dd.h"
#include "dict0dict.h"
#include "dict0sdi.h"
#include "api0api.h"
#include "api0misc.h"
#include "trx0trx.h"
#include "ha_innodb.h"
#include "dict0sdi-decompress.h"
#include "fsp0fsp.h"

/** Check for existence of SDI copies in a tablespace
@param[in]	tablespace	tablespace object
@param[in,out]	space_id	space_id from tablespace object
@return DB_SUCESS if SDI exists, else return DB_ERROR */
static
dberr_t
dict_sdi_check_existence(
	const dd::Tablespace&	tablespace,
	uint32*			space_id)
{
	if (tablespace.se_private_data().get_uint32("id", space_id)) {
		/* error, attribute not found */
		ut_ad(0);
		return(DB_ERROR);
	}

	ut_ad(check_trx_exists(current_thd) != NULL);

	if (fsp_is_undo_tablespace(*space_id)
	    || fsp_is_system_temporary(*space_id)) {
		/* Claim Success */
		return(DB_SUCCESS);
	}

	return(fsp_has_sdi(*space_id) ? DB_SUCCESS : DB_ERROR);
}

/** Create SDI in a tablespace. This API should be used when
upgrading a tablespace with no SDI.
@param[in,out]	tablespace	tablespace object
@retval		false		success
@retval		true		failure */
bool
dict_sdi_create(
	dd::Tablespace*	tablespace)
{
	DBUG_EXECUTE_IF("ib_sdi",
		ib::info() << "SDI_CREATE: dict_sdi_create("
			<< tablespace->name() << ","
			<< tablespace->id() << ")";
	);

	uint32	space_id;
	if (tablespace->se_private_data().get_uint32("id", &space_id)) {
		/* error, attribute not found */
		ut_ad(0);
		return(true);
	}

	if (fsp_is_undo_tablespace(space_id)
	    || fsp_is_system_temporary(space_id)) {
		/* Upgrade calls sdi_create on dd::Tablespaces
		registered. We shouldn't create SDI for
		undo and temporary tablespace. */
		return(false);
	}

	dberr_t	err = ib_sdi_create(space_id);

	/* If SDI index is created, update the tablespace flags in
	dictionary */
	if (err == DB_SUCCESS) {
		fil_space_t* 	space = fil_space_acquire(space_id);
		ut_ad(space != nullptr);

		dd::Properties& p = tablespace->se_private_data();
		p.set_uint32(dd_space_key_strings[DD_SPACE_FLAGS],
			     static_cast<uint32>(space->flags));

		fil_space_release(space);
	}

	return(err != DB_SUCCESS);
}

/** Drop SDI in a tablespace. This API should be used only
when SDI is corrupted.
@param[in,out]	tablespace	tablespace object
@retval		false		success
@retval		true		failure */
bool
dict_sdi_drop(dd::Tablespace*	tablespace)
{
#if 0 /* TODO: Enable in WL#9761 */
	uint32	space_id;
	if (dict_sdi_check_existence(tablespace, &space_id)
	    != DB_SUCCESS) {
		return(true);
	}

	dberr_t	err = ib_sdi_drop(space_id);
	return(err != DB_SUCCESS);
#endif /* TODO: Enable in WL#9761 */
	ut_ad(0);
	return(false);
}

/** Get the SDI keys in a tablespace into the vector provided.
@param[in]	tablespace	tablespace object
@param[in,out]	vector		vector to hold SDI keys
@retval		false		success
@retval		true		failure */
bool
dict_sdi_get_keys(
	const dd::Tablespace&	tablespace,
	dd::sdi_vector_t&	vector)
{
#if 0 /* TODO: Enable in WL#9761 */
	uint32	space_id;

	if (dd_tablespace_get_discard(&tablespace)) {
		/* sdi_get_keys shouldn't be called on discarded tablespaces.*/
		ut_ad(0);
	}

	if (dict_sdi_check_existence(tablespace, &space_id)
	    != DB_SUCCESS) {
		return(true);
	}

#ifdef UNIV_DEBUG
	if (fsp_is_undo_tablespace(space_id)
	    || fsp_is_system_temporary(space_id)) {
		/* There shouldn't be access for SDI on these tabelspaces as
		SDI doesn't exist. */
		ut_ad(0);
	}
#endif /* UNIV_DEBUG */

	ib_sdi_vector	ib_vector;
	ib_vector.sdi_vector = &vector;

	trx_t*	trx = check_trx_exists(current_thd);
	trx_start_if_not_started(trx, true);

	dberr_t	err = ib_sdi_get_keys(space_id, &ib_vector, trx);

	return(err != DB_SUCCESS);
#endif /* TODO: Enable in WL#9761 */
	ut_ad(0);
	return(false);
}

/** Retrieve SDI from tablespace
@param[in]	tablespace	tablespace object
@param[in]	sdi_key		SDI key
@param[in,out]	sdi		SDI retrieved from tablespace
@param[in,out]	sdi_len		in:  size of memory allocated
				out: actual length of SDI
@retval		false		success
@retval		true		failure */
bool
dict_sdi_get(
	const dd::Tablespace&	tablespace,
	const dd::sdi_key_t*	sdi_key,
	void*			sdi,
	uint64*			sdi_len)
{

#if 0 /* TODO: Enable in WL#9761 */
	DBUG_EXECUTE_IF("ib_sdi",
		ib::info() << "dict_sdi_get(" << tablespace.name()
			<< "," << tablespace.id()
			<< " sdi_key: type: " << sdi_key->type
			<< " id: " << sdi_key->id
			<< ")";
	);

	if (dd_tablespace_get_discard(&tablespace)) {
		/* sdi_get shouldn't be called on discarded tablespaces.*/
		ut_ad(0);
	}

	uint32	space_id;
	if (dict_sdi_check_existence(tablespace, &space_id)
	    != DB_SUCCESS) {
		return(true);
	}

#ifdef UNIV_DEBUG
	if (fsp_is_undo_tablespace(space_id)
	    || fsp_is_system_temporary(space_id)) {
		/* There shouldn't be access for SDI on these tabelspaces as
		SDI doesn't exist. */
		ut_ad(0);
	}
#endif /* UNIV_DEBUG */

	trx_t*	trx = check_trx_exists(current_thd);
	trx_start_if_not_started(trx, true);

	ib_sdi_key_t	ib_sdi_key;
	ib_sdi_key.sdi_key = sdi_key;

	ut_ad(*sdi_len < UINT32_MAX);
	uint32_t	uncompressed_sdi_len;
	uint32_t	compressed_sdi_len = static_cast<uint32_t>(*sdi_len);
	byte*		compressed_sdi = static_cast<byte*>(
		ut_malloc_nokey(compressed_sdi_len));

	dberr_t	err = ib_sdi_get(
		space_id, &ib_sdi_key,
		compressed_sdi, &compressed_sdi_len,
		&uncompressed_sdi_len,
		trx);

	if (err == DB_OUT_OF_MEMORY) {
		*sdi_len = uncompressed_sdi_len;
	} else if (err != DB_SUCCESS) {
		*sdi_len = UINT64_MAX;
	} else {
		*sdi_len = uncompressed_sdi_len;
		/* Decompress the data */
		Sdi_Decompressor decompressor(static_cast<byte*>(sdi),
					      uncompressed_sdi_len,
					      compressed_sdi,
					      compressed_sdi_len);
		decompressor.decompress();
        }

	ut_free(compressed_sdi);

	return(err != DB_SUCCESS);
#endif /* TODO: Enable in WL#9761 */
	ut_ad(0);
	return(false);
}

/** Insert/Update SDI in tablespace
@param[in]	tablespace	tablespace object
@param[in]	table		table object
@param[in]	sdi_key		SDI key to uniquely identify the tablespace
object
@param[in]	sdi		SDI to be stored in tablespace
@param[in]	sdi_len		SDI length
@retval		false		success
@retval		true		failure */
bool
dict_sdi_set(
	const dd::Tablespace&	tablespace,
	const dd::Table*	table,
	const dd::sdi_key_t*	sdi_key,
	const void*		sdi,
	uint64			sdi_len)
{
	DBUG_EXECUTE_IF("ib_sdi",
		ib::info() << "dict_sdi_set(" << tablespace.name()
			<< "," << tablespace.id()
			<< " sdi_key: type: " << sdi_key->type
			<< " id: " << sdi_key->id
			<< ")";
	);

	/* Used for testing purpose for DDLs from Memcached */
	DBUG_EXECUTE_IF("skip_sdi", return(false););

	if (dd_tablespace_get_discard(&tablespace)) {
		/* Claim success. */
		return(false);
	}

	/* Check if dd:Table has valid se_private_id. In case of partitions,
	all partitions should have valid se_private_id. If not, we cannot
	proceed with storing SDI as the tablespace is not created yet. */
	if (table && (table->se_private_id() == dd::INVALID_OBJECT_ID)
	    && std::all_of(table->partitions().begin(), table->partitions().end(),
			   [](const dd::Partition *p)
			   {return(p->se_private_id() == dd::INVALID_OBJECT_ID);}
			   ))
	{
		/* This is a preliminary store of the object - before SE has
		added SE-specific data. Cannot, and should not, store sdi at
		this point. We should not throw error here. There will be SDI
		store again with valid se_private_id/data */
		DBUG_EXECUTE_IF("ib_sdi",
			ib::info() << "dict_sdi_set(" << tablespace.name()
				<< "," << tablespace.id()
				<< " sdi_key: type: " << sdi_key->type
				<< " id: " << sdi_key->id
				<< "): invalid se_private_id";
		);

		return(false);
	}

	if (!tablespace.se_private_data().exists("id")) {
		/* Claim success, there will be one more sdi_set()
		after the tablespace is created. */
		return(false);
	}

	uint32	space_id;
	if (dict_sdi_check_existence(tablespace, &space_id)
	    != DB_SUCCESS) {
		ut_ad(0);
		return(true);
	}

	if (fsp_is_undo_tablespace(space_id)
	    || fsp_is_system_temporary(space_id)) {
		/* Claim Success */
		return(false);
	}

	trx_t*	trx = check_trx_exists(current_thd);
	trx_start_if_not_started(trx, true);

	ib_sdi_key_t	ib_sdi_key;
	ib_sdi_key.sdi_key = sdi_key;

	Sdi_Compressor	compressor(sdi_len, sdi);
	compressor.compress();

        dberr_t	err = ib_sdi_set(space_id, &ib_sdi_key, sdi_len,
				 compressor.get_comp_len(),
				 compressor.get_data(), trx);

	if (err == DB_INTERRUPTED) {
		my_error(ER_QUERY_INTERRUPTED, MYF(0));
		DBUG_EXECUTE_IF("ib_sdi",
		ib::info() << "dict_sdi_set: " << tablespace.name()
				<< "," << tablespace.id()
				<< " InnoDB space_id: " << space_id
				<< " sdi_key: type: " << sdi_key->type
				<< " id: " << sdi_key->id
				<< " trx_id: " << trx->id
				<< " is interrupted";
		);
		return(true);
	} else if (err != DB_SUCCESS) {
		ut_ad(0);
		return(true);
	} else {
		return(false);
	}
}

/** @return true if query is DROP TABLE, else false */
static
bool
thd_is_drop_table(THD*	thd)
{
	return(thd != nullptr
	       && (thd_sql_command(thd) == SQLCOM_DROP_TABLE));
}

/** Delete SDI from tablespace
@param[in]	tablespace	tablespace object
@param[in]	table		table object
@param[in]	sdi_key		SDI key to uniquely identify the tablespace
				object
@retval		false		success
@retval		true		failure */
bool
dict_sdi_delete(
	const dd::Tablespace&	tablespace,
	const dd::Table*	table,
	const dd::sdi_key_t*	sdi_key)
{
	DBUG_EXECUTE_IF("ib_sdi",
		ib::info() << "dict_sdi_delete(" << tablespace.name()
			<< "," << tablespace.id()
			<< " sdi_key: type: " << sdi_key->type
			<< " id: " << sdi_key->id
			<< ")";
	);

	/* Used for testing purpose for DDLs from Memcached */
	DBUG_EXECUTE_IF("skip_sdi", return(false););

	if (dd_tablespace_get_discard(&tablespace)) {
		/* Claim success. */
		return(false);
	}

	/* Check if dd:Table has valid se_private_id. In case of partitions,
	all partitions should have valid se_private_id. If not, we cannot
	proceed with storing SDI as the tablespace is not created yet. */
	if (table && (table->se_private_id() == dd::INVALID_OBJECT_ID)
	    && std::all_of(table->partitions().begin(), table->partitions().end(),
			   [](const dd::Partition *p)
			   {return(p->se_private_id() == dd::INVALID_OBJECT_ID);}
			   ))
	{
		/* This is a preliminary store of the object - before SE has
		added SE-specific data. Cannot, and should not, store sdi at
		this point. We should not throw error here. There will be SDI
		store again with valid se_private_id/data */

		DBUG_EXECUTE_IF("ib_sdi",
			ib::info() << "dict_sdi_delete(" << tablespace.name()
				<< "," << tablespace.id()
				<< " sdi_key: type: " << sdi_key->type
				<< " id: " << sdi_key->id
				<< "): invalid se_private_id";
		);
		return(false);
	}

	uint32	space_id;
	if (dict_sdi_check_existence(tablespace, &space_id)
	    != DB_SUCCESS) {

		if (thd_is_drop_table(current_thd)) {
			/* Claim Success */
			return(false);
		} else {
			ut_ad(0);
			return(true);
		}
	}

	if (fsp_is_undo_tablespace(space_id)
	    || fsp_is_system_temporary(space_id)) {
		/* Claim Success */
		return(false);
	}

	trx_t*	trx = check_trx_exists(current_thd);
	trx_start_if_not_started(trx, true);

	ib_sdi_key_t	ib_sdi_key;
	ib_sdi_key.sdi_key = sdi_key;

	dberr_t	err = ib_sdi_delete(space_id, &ib_sdi_key, trx);

	if (err == DB_INTERRUPTED) {
		my_error(ER_QUERY_INTERRUPTED, MYF(0));

		DBUG_EXECUTE_IF("ib_sdi",
		ib::info() << "dict_sdi_delete(" << tablespace.name()
				<< "," << tablespace.id()
				<< " InnoDB space_id: " << space_id
				<< " sdi_key: type: " << sdi_key->type
				<< " id: " << sdi_key->id
				<< " trx_id: " << trx->id
				<< " is interrupted";
		);
		return(true);
	} else if (err != DB_SUCCESS) {
		ut_ad(0);
		return(true);
	} else {
		return(false);
	}
}
