/*****************************************************************************

Copyright (c) 2017, 2022, Oracle and/or its affiliates.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License, version 2.0, as published by the
Free Software Foundation.

This program is also distributed with certain software (including but not
limited to OpenSSL) that is licensed under separate terms, as designated in a
particular file or component or in included license documentation. The authors
of MySQL hereby grant you an additional permission to link the program and
your derivative works with the separately licensed software that they have
included with MySQL.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License, version 2.0,
for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

*****************************************************************************/

#include <algorithm>

#include <current_thd.h>
#include <sql_class.h>
#include <sql_show.h>
#include <sql_table.h>
#include <sql_tablespace.h>
#include "api0api.h"
#include "api0misc.h"
#include "dict0boot.h"
#include "dict0crea.h"
#include "dict0dd.h"
#include "dict0dict.h"
#include "dict0sdi-decompress.h"
#include "dict0sdi.h"
#include "fsp0fsp.h"
#include "ha_innodb.h"
#include "trx0trx.h"

/** Check if SDI Index exists in a tablespace
@param[in]      dd_space  DD tablespace object
@param[in,out]  space_id  space_id from tablespace object
@return DB_SUCCESS if SDI exists, else return DB_ERROR,
DB_TABLESPACE_NOT_FOUND */
static dberr_t dict_sdi_exists(const dd::Tablespace &dd_space,
                               uint32_t *space_id) {
  if (dd_space.se_private_data().get(dd_space_key_strings[DD_SPACE_ID],
                                     space_id)) {
    /* error, attribute not found */
    ut_d(ut_error);
    ut_o(return (DB_ERROR));
  }

  ut_ad(check_trx_exists(current_thd) != nullptr);

  if (fsp_is_undo_tablespace(*space_id) || fsp_is_system_temporary(*space_id)) {
    /* Claim Success */
    return (DB_SUCCESS);
  }

  return (fsp_has_sdi(*space_id));
}

/** Report error on failure
@param[in]      errornum        MySQL error number (for my_error()) (Must take
                                4 string arguments, in the same way as
                                ER_SDI_OPERATION_FAILED)
@param[in]      operation       SDI set or delete
@param[in]      table           table object for which SDI is serialized
@param[in]      tablespace      tablespace where SDI is stored */
static void dict_sdi_report_error(int errornum, const char *operation,
                                  const dd::Table *table,
                                  const dd::Tablespace &tablespace) {
  THD *thd = current_thd;
  const char *schema_name = nullptr;
  const dd::Schema *schema = nullptr;
  const char *table_name = nullptr;

  if (thd != nullptr && table != nullptr) {
    table_name = table->name().c_str();
    if (thd->dd_client()->acquire(table->schema_id(), &schema)) {
      schema_name = nullptr;
    } else {
      schema_name = schema->name().c_str();
    }
  }

  if (schema_name == nullptr) {
    schema_name = "<no schema>";
  }

  if (table_name == nullptr) {
    table_name = "<no table>";
  }

  my_error(errornum, MYF(0), operation, schema_name, table_name,
           tablespace.name().c_str());
}

/** Report error on failure. Calls dict_sdi_report_error(int errornum,
const char *operation, const dd::Table *table, const
dd::Tablespace &tablespace) with errornum=SDI_OPERATION_FAILED
(for compatibility with existing code).

@param[in]      operation       SDI set or delete
@param[in]      table           table object for which SDI is serialized
@param[in]      tablespace      tablespace where SDI is stored */
static void dict_sdi_report_error(const char *operation, const dd::Table *table,
                                  const dd::Tablespace &tablespace) {
  dict_sdi_report_error(ER_SDI_OPERATION_FAILED, operation, table, tablespace);
}

/** Create SDI in a tablespace. This API should be used when
upgrading a tablespace with no SDI.
@param[in,out]  tablespace      tablespace object
@retval         false           success
@retval         true            failure */
bool dict_sdi_create(dd::Tablespace *tablespace) {
  DBUG_EXECUTE_IF("ib_sdi", ib::info(ER_IB_MSG_213)
                                << "SDI_CREATE: dict_sdi_create("
                                << tablespace->name() << "," << tablespace->id()
                                << ")";);

  uint32_t space_id = 0;
  if (tablespace->se_private_data().get("id", &space_id)) {
    /* error, attribute not found */
    ut_d(ut_error);
    ut_o(return (true));
  }

  if (fsp_is_undo_tablespace(space_id) || fsp_is_system_temporary(space_id)) {
    /* Upgrade calls sdi_create on dd::Tablespaces
    registered. We shouldn't create SDI for
    undo and temporary tablespace. */
    return (false);
  }

  dberr_t err = ib_sdi_create(space_id);

  /* If SDI index is created, update the tablespace flags in
  dictionary */
  if (err == DB_SUCCESS) {
    fil_space_t *space = fil_space_acquire(space_id);
    ut_ad(space != nullptr);

    dd::Properties &p = tablespace->se_private_data();
    p.set(dd_space_key_strings[DD_SPACE_FLAGS],
          static_cast<uint32>(space->flags));

    fil_space_release(space);
  }

  return (err != DB_SUCCESS);
}

/** Drop SDI in a tablespace. This API should be used only
when SDI is corrupted.
@param[in,out]  tablespace      tablespace object
@retval         false           success
@retval         true            failure */
bool dict_sdi_drop(dd::Tablespace *) {
#if 0  /* TODO: Enable in WL#9761 */
        uint32  space_id;
        if (dict_sdi_exists(tablespace, &space_id)
            != DB_SUCCESS) {
                return(true);
        }

        dberr_t err = ib_sdi_drop(space_id);
        return(err != DB_SUCCESS);
#endif /* TODO: Enable in WL#9761 */
  ut_d(ut_error);
  ut_o(return (false));
}

/** Get the SDI keys in a tablespace into the vector provided.
@param[in]      tablespace      tablespace object
@param[in,out]  vector          vector to hold SDI keys
@retval         false           success
@retval         true            failure */
bool dict_sdi_get_keys(const dd::Tablespace &tablespace, sdi_vector_t &vector) {
  uint32_t space_id;

  if (dd_tablespace_is_discarded(&tablespace)) {
    /* sdi_get_keys shouldn't be called on discarded tablespaces.*/
    my_error(ER_SDI_GET_KEYS_INVALID_TABLESPACE, MYF(0),
             tablespace.name().c_str());
    ut_d(ut_error);
    ut_o(return true);
  }

  if (dict_sdi_exists(tablespace, &space_id) != DB_SUCCESS) {
    my_error(ER_SDI_GET_KEYS_INVALID_TABLESPACE, MYF(0),
             tablespace.name().c_str());
    return true;
  }

  if (fsp_is_undo_tablespace(space_id) || fsp_is_system_temporary(space_id)) {
    /* There shouldn't be access for SDI on these tablespaces as
    SDI doesn't exist. */

    my_error(ER_SDI_GET_KEYS_INVALID_TABLESPACE, MYF(0),
             tablespace.name().c_str());
    ut_d(ut_error);
    ut_o(return true);
  }

  ib_sdi_vector ib_vector;
  ib_vector.sdi_vector = &vector;

  trx_t *trx = check_trx_exists(current_thd);
  trx_start_if_not_started(trx, true, UT_LOCATION_HERE);

  dberr_t err = ib_sdi_get_keys(space_id, &ib_vector, trx);

  return (err != DB_SUCCESS);
}

bool dict_sdi_get(const dd::Tablespace &, const sdi_key_t *, void *,
                  uint64_t *) {
#if 0 /* TODO: Enable in WL#9761 */
        DBUG_EXECUTE_IF("ib_sdi",
                ib::info(ER_IB_MSG_214) << "dict_sdi_get(" << tablespace.name()
                        << "," << tablespace.id()
                        << " sdi_key: type: " << sdi_key->type
                        << " id: " << sdi_key->id
                        << ")";
        );

        if (dd_tablespace_is_discarded(&tablespace)) {
                /* sdi_get shouldn't be called on discarded tablespaces.*/
                ut_d(ut_error);
        }

        uint32  space_id;
        if (dict_sdi_exists(tablespace, &space_id)
            != DB_SUCCESS) {
                return(true);
        }

#ifdef UNIV_DEBUG
        if (fsp_is_undo_tablespace(space_id)
            || fsp_is_system_temporary(space_id)) {
                /* There shouldn't be access for SDI on these tablespaces as
                SDI doesn't exist. */
                ut_d(ut_error);
        }
#endif /* UNIV_DEBUG */

        trx_t*  trx = check_trx_exists(current_thd);
        trx_start_if_not_started(trx, true, UT_LOCATION_HERE);

        ib_sdi_key_t    ib_sdi_key;
        ib_sdi_key.sdi_key = sdi_key;

        ut_ad(*sdi_len < UINT32_MAX);
        uint32_t        uncompressed_sdi_len;
        uint32_t        compressed_sdi_len = static_cast<uint32_t>(*sdi_len);
        byte*           compressed_sdi = static_cast<byte*>(
                ut::malloc_withkey(UT_NEW_THIS_FILE_PSI_KEY, compressed_sdi_len));

        dberr_t err = ib_sdi_get(
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

        ut::free(compressed_sdi);

        return(err != DB_SUCCESS);
#endif /* TODO: Enable in WL#9761 */
  ut_d(ut_error);
  ut_o(return (false));
}

/** Insert/Update SDI in tablespace
@param[in]      hton            handlerton object
@param[in]      tablespace      tablespace object
@param[in]      table           table object
@param[in]      sdi_key         SDI key to uniquely identify the tablespace
object
@param[in]      sdi             SDI to be stored in tablespace
@param[in]      sdi_len         SDI length
@retval         false           success
@retval         true            failure */
bool dict_sdi_set(handlerton *hton, const dd::Tablespace &tablespace,
                  const dd::Table *table, const sdi_key_t *sdi_key,
                  const void *sdi, uint64_t sdi_len) {
  const char *operation = "set";

  DBUG_EXECUTE_IF("ib_sdi", ib::info(ER_IB_MSG_215)
                                << "dict_sdi_set(" << tablespace.name() << ","
                                << tablespace.id()
                                << " sdi_key: type: " << sdi_key->type
                                << " id: " << sdi_key->id << ")";);

  /* Used for testing purpose for DDLs from Memcached */
  DBUG_EXECUTE_IF("skip_sdi", return (false););

  if (dd_tablespace_is_discarded(&tablespace)) {
    /* Claim success. */
    return (false);
  }

  /* Check if dd:Table has valid se_private_id. In case of partitions,
  all partitions should have valid se_private_id. If not, we cannot
  proceed with storing SDI as the tablespace is not created yet. */
  if (table && (table->se_private_id() == dd::INVALID_OBJECT_ID) &&
      std::all_of(table->leaf_partitions().begin(),
                  table->leaf_partitions().end(), [](const dd::Partition *lp) {
                    return (lp->se_private_id() == dd::INVALID_OBJECT_ID);
                  })) {
    /* This is a preliminary store of the object - before SE has
    added SE-specific data. Cannot, and should not, store sdi at
    this point. We should not throw error here. There will be SDI
    store again with valid se_private_id/data */
    DBUG_EXECUTE_IF(
        "ib_sdi", ib::info(ER_IB_MSG_216)
                      << "dict_sdi_set(" << tablespace.name() << ","
                      << tablespace.id() << " sdi_key: type: " << sdi_key->type
                      << " id: " << sdi_key->id << "): invalid se_private_id";);

    return (false);
  }

  if (!tablespace.se_private_data().exists(dd_space_key_strings[DD_SPACE_ID])) {
    /* Claim success, there will be one more sdi_set()
    after the tablespace is created. */
    return (false);
  }

  uint32_t space_id;
  dberr_t err = dict_sdi_exists(tablespace, &space_id);
  if (err != DB_SUCCESS) {
    if (err == DB_TABLESPACE_NOT_FOUND) {
      /* Claim Success */
      return (false);
    } else {
      dict_sdi_report_error(operation, table, tablespace);
      ut_d(ut_error);
      ut_o(return (true));
    }
  }

  if (fsp_is_undo_tablespace(space_id) || fsp_is_system_temporary(space_id)) {
    /* Claim Success */
    return (false);
  }

  trx_t *trx = check_trx_exists(current_thd);
  trx_start_if_not_started(trx, true, UT_LOCATION_HERE);

  innobase_register_trx(hton, current_thd, trx);

  ib_sdi_key_t ib_sdi_key;
  ib_sdi_key.sdi_key = sdi_key;

  Sdi_Compressor compressor(static_cast<uint32_t>(sdi_len), sdi);
  compressor.compress();

  err = ib_sdi_set(space_id, &ib_sdi_key, static_cast<uint32_t>(sdi_len),
                   compressor.get_comp_len(), compressor.get_data(), trx);

  DBUG_EXECUTE_IF("sdi_set_failure",
                  dict_sdi_report_error(operation, table, tablespace);
                  return (true););

  if (err == DB_INTERRUPTED) {
    my_error(ER_QUERY_INTERRUPTED, MYF(0));
    DBUG_EXECUTE_IF("ib_sdi",
                    ib::info(ER_IB_MSG_217)
                        << "dict_sdi_set: " << tablespace.name() << ","
                        << tablespace.id() << " InnoDB space_id: " << space_id
                        << " sdi_key: type: " << sdi_key->type
                        << " id: " << sdi_key->id << " trx_id: " << trx->id
                        << " is interrupted";);
    return (true);
  } else if (err != DB_SUCCESS) {
    dict_sdi_report_error(operation, table, tablespace);
    ut_d(ut_error);
    ut_o(return (true));
  } else {
    return (false);
  }
}

/** Delete SDI from tablespace
@param[in]      tablespace      tablespace object
@param[in]      table           table object
@param[in]      sdi_key         SDI key to uniquely identify the tablespace
                                object
@retval         false           success
@retval         true            failure */
bool dict_sdi_delete(const dd::Tablespace &tablespace, const dd::Table *table,
                     const sdi_key_t *sdi_key) {
  const char *operation = "delete";

  DBUG_EXECUTE_IF("ib_sdi", ib::info(ER_IB_MSG_218)
                                << "dict_sdi_delete(" << tablespace.name()
                                << "," << tablespace.id()
                                << " sdi_key: type: " << sdi_key->type
                                << " id: " << sdi_key->id << ")";);

  /* Used for testing purpose for DDLs from Memcached */
  DBUG_EXECUTE_IF("skip_sdi", return (false););

  if (dd_tablespace_is_discarded(&tablespace)) {
    /* Claim success. */
    return (false);
  }

  /* Check if dd:Table has valid se_private_id. In case of partitions,
  all partitions should have valid se_private_id. If not, we cannot
  proceed with storing SDI as the tablespace is not created yet. */
  if (table && (table->se_private_id() == dd::INVALID_OBJECT_ID) &&
      std::all_of(table->leaf_partitions().begin(),
                  table->leaf_partitions().end(), [](const dd::Partition *p) {
                    return (p->se_private_id() == dd::INVALID_OBJECT_ID);
                  })) {
    /* This is a preliminary store of the object - before SE has
    added SE-specific data. Cannot, and should not, store sdi at
    this point. We should not throw error here. There will be SDI
    store again with valid se_private_id/data */

    DBUG_EXECUTE_IF(
        "ib_sdi", ib::info(ER_IB_MSG_219)
                      << "dict_sdi_delete(" << tablespace.name() << ","
                      << tablespace.id() << " sdi_key: type: " << sdi_key->type
                      << " id: " << sdi_key->id << "): invalid se_private_id";);
    return (false);
  }

  uint32_t space_id;
  dberr_t err = dict_sdi_exists(tablespace, &space_id);
  if (err != DB_SUCCESS) {
    if (err == DB_TABLESPACE_NOT_FOUND) {
      /* Claim Success */
      return (false);
    } else {
      dict_sdi_report_error(operation, table, tablespace);
      ut_d(ut_error);
      ut_o(return (true));
    }
  }

  if (fsp_is_undo_tablespace(space_id) || fsp_is_system_temporary(space_id)) {
    /* Claim Success */
    return (false);
  }

  trx_t *trx = check_trx_exists(current_thd);
  trx_start_if_not_started(trx, true, UT_LOCATION_HERE);

  ib_sdi_key_t ib_sdi_key;
  ib_sdi_key.sdi_key = sdi_key;

  err = ib_sdi_delete(space_id, &ib_sdi_key, trx);

  DBUG_EXECUTE_IF("sdi_delete_failure",
                  dict_sdi_report_error(operation, table, tablespace);
                  return (true););

  if (err == DB_INTERRUPTED) {
    my_error(ER_QUERY_INTERRUPTED, MYF(0));

    DBUG_EXECUTE_IF("ib_sdi",
                    ib::info(ER_IB_MSG_220)
                        << "dict_sdi_delete(" << tablespace.name() << ","
                        << tablespace.id() << " InnoDB space_id: " << space_id
                        << " sdi_key: type: " << sdi_key->type
                        << " id: " << sdi_key->id << " trx_id: " << trx->id
                        << " is interrupted";);
    return (true);
  } else if (err != DB_SUCCESS) {
    if (err == DB_RECORD_NOT_FOUND)
      dict_sdi_report_error(ER_SDI_OPERATION_FAILED_MISSING_RECORD, operation,
                            table, tablespace);
    else
      dict_sdi_report_error(operation, table, tablespace);
    return (true);
  } else {
    return (false);
  }
}
