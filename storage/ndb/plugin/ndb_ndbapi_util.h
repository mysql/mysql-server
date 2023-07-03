/*
   Copyright (c) 2011, 2022, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef NDB_NDBAPI_UTIL_H
#define NDB_NDBAPI_UTIL_H

#include <stddef.h>
#include <functional>
#include <string>
#include <unordered_set>
#include <vector>

#include "storage/ndb/include/ndbapi/NdbBlob.hpp"
#include "storage/ndb/include/ndbapi/NdbDictionary.hpp"
#include "storage/ndb/include/ndbapi/NdbRecAttr.hpp"

class THD;
class NdbScanFilter;

union NdbValue {
  const NdbRecAttr *rec;
  NdbBlob *blob;
  void *ptr;
};

/**
 * @brief ndb_pack_varchar, pack the given string using "MySQL Server varchar
 *        format" into a buffer suitable for the given column of the NDB table
 * @param ndbtab         NDB table
 * @param column_index   index of column to pack for
 * @param str            string to pack
 * @param str_length     length of string to pack
 *
 * @note The hardcoded value 512 is the current size of FN_REFLEN, only buffers
 *       of that size is currently supported by this function
 */
void ndb_pack_varchar(const NdbDictionary::Table *ndbtab, unsigned column_index,
                      char (&buf)[512], const char *str, size_t str_length);

/**
 * @brief ndb_pack_varchar, pack the given string using "MySQL Server varchar
 *        format" into a buffer suitable for the given column of the NDB table
 * @param column         NDB Column
 * @param offset         position of this column's data in buffer
 * @param str            string to pack
 * @param str_length     length of string to pack
 * @param buf            pointer to data buffer of any size
 */
void ndb_pack_varchar(const NdbDictionary::Column *column, size_t offset,
                      const char *str, size_t str_length, char *buf);

/**
 * @brief ndb_unpack_varchar, retrieve a pointer and string length from
 *        a data buffer. Assumes that the caller has already verified that
 *        the stored value is non-null.
 * @param column         NDB Column
 * @param offset         position of this column's data in buffer
 * @param str            string destination (out)
 * @param str_length     string length destination (out)
 * @param buf            pointer to filled data buffer
 */
void ndb_unpack_varchar(const NdbDictionary::Column *column, size_t offset,
                        const char **str, size_t *str_length, const char *buf);

/**
   @brief ndb_get_extra_metadata_version, returns the version of the
          extra metadata attached to the table in NDB.
   @return version of extra metadata or 0 if none
 */
Uint32 ndb_get_extra_metadata_version(const NdbDictionary::Table *ndbtab);

/**
   @brief returns serialized metadata attached to the
   table in NDB.

   @param ndbtab The NDB table
   @param[out] serialized_metadata variable to receive the serialized metadata

   @return true if table has extra metadata version 2
*/
bool ndb_table_get_serialized_metadata(const NdbDictionary::Table *ndbtab,
                                       std::string &serialized_metadata);

/**
 * @brief ndb_table_has_blobs, check if the NDB table has blobs
 * @return true if the table have blobs
 */
bool ndb_table_has_blobs(const NdbDictionary::Table *ndbtab);

/**
 * @brief ndb_table_has_hidden_pk, check if the NDB table has a hidden
 *        primary key(as created by ndbcluster to support having table
 *        without primary key in NDB)
 * @return true if the table has a hidden primary key
 */
bool ndb_table_has_hidden_pk(const NdbDictionary::Table *ndbtab);

/**
 * @brief check if the NDB table has tablespace
 * @return true if the table has a tablespace
 *
 * @note This is indicated either by the table having a tablespace name
 *       or id+version of the tablespace
 */
bool ndb_table_has_tablespace(const NdbDictionary::Table *ndbtab);

/**
 * @brief check if the NDB table has tablespace name indicating
 *        that is has a tablespace
 * @return nullptr or tablespace name
 *
 * @note The NdbApi function getTablespaceName() is peculiar as it
 *       returns the empty string to indicate that tablespace name
 *       is not available, normally you'd expect NULL to be returned
 *       from a function returning "const char*"
 *
 */
const char *ndb_table_tablespace_name(const NdbDictionary::Table *ndbtab);

/**
 * @brief Return the tablespace name of an NDB table
 * @param dict    NDB Dictionary
 * @param ndbtab  NDB Table object
 * @return tablespace name if table has tablespace, empty string if not
 */
std::string ndb_table_tablespace_name(NdbDictionary::Dictionary *dict,
                                      const NdbDictionary::Table *ndbtab);

/**
 * @brief Checks if an error has occurred in a ndbapi call
 * @param dict  NDB Dictionary
 * @return true if error has occurred, false if not
 */
bool ndb_dict_check_NDB_error(NdbDictionary::Dictionary *dict);

/**
 * @brief Retrieves list of logfile group names from NDB Dictionary
 * @param dict            NDB Dictionary
 * @param lfg_names [out] List of logfile group names
 * @return true on success, false on failure
 */
bool ndb_get_logfile_group_names(const NdbDictionary::Dictionary *dict,
                                 std::unordered_set<std::string> &lfg_names);

/**
 * @brief Retrieves list of tablespace names from NDB Dictionary
 * @param dict                   NDB Dictionary
 * @param tablespace_names [out] List of tablespace names
 * @return true on success, false on failure
 */
bool ndb_get_tablespace_names(
    const NdbDictionary::Dictionary *dict,
    std::unordered_set<std::string> &tablespace_names);

/**
 * @brief Retrieves list of table names in the given schema from NDB Dictionary
 * @param dict              NDB Dictionary
 * @param schema_name       Schema name
 * @param table_names [out] List of table names
 * @param temp_names [out]  Optional, set of temporary table names
 *
 * @return true on success, false on failure
 */
bool ndb_get_table_names_in_schema(
    const NdbDictionary::Dictionary *dict, const std::string &schema_name,
    std::unordered_set<std::string> *table_names,
    std::unordered_set<std::string> *temp_names = nullptr);

/**
 * @brief Retrieves list of undofile names assigned to a logfile group from NDB
 *        Dictionary
 * @param dict                 NDB Dictionary
 * @param logfile_group_name   Logfile group name
 * @param undofile_names [out] Undofile names
 * @return true on success, false on failure
 */
bool ndb_get_undofile_names(NdbDictionary::Dictionary *dict,
                            const std::string &logfile_group_name,
                            std::vector<std::string> *undofile_names);

/**
 * @brief Retrieves list of datafile names assigned to a tablespace from NDB
 *        Dictionary
 * @param dict                 NDB Dictionary
 * @param tablespace_name      Tablespace name
 * @param datafile_names [out] Datafile names
 * @return true on success, false on failure
 */
bool ndb_get_datafile_names(NdbDictionary::Dictionary *dict,
                            const std::string &tablespace_name,
                            std::vector<std::string> *datafile_names);

/**
 * @brief Retrieves list of database names in NDB Dictionary
 * @param dict                 NDB Dictionary
 * @param database_names [out] List of database names in Dictionary
 * @return true on success, false on failure
 */
bool ndb_get_database_names_in_dictionary(
    NdbDictionary::Dictionary *dict,
    std::unordered_set<std::string> *database_names);

/**
 * @brief Checks if a database is being used in NDB Dictionary
 * @param dict           NDB Dictionary
 * @param database_name  Name of database being checked
 * @param exists [out]   True if database exists, false if not
 * @return true on success, false on failure
 */
bool ndb_database_exists(NdbDictionary::Dictionary *dict,
                         const std::string &database_name, bool &exists);

/**
 * @brief Check if a logfile group exists in NDB Dictionary
 * @param dict                 NDB Dictionary
 * @param logfile_group_name   Logfile group name
 * @param exists [out]         Boolean that is set to true if the logfile group
 *                             exists, false if not
 * @return true on success, false on failure
 */
bool ndb_logfile_group_exists(NdbDictionary::Dictionary *dict,
                              const std::string &logfile_group_name,
                              bool &exists);

/**
 * @brief Check if a tablespace exists in NDB Dictionary
 * @param dict              NDB Dictionary
 * @param tablespace_name   Tablespace name
 * @param exists [out]      Boolean that is set to true if the tablespace
 *                          exists, false if not
 * @return true on success, false on failure
 */
bool ndb_tablespace_exists(NdbDictionary::Dictionary *dict,
                           const std::string &tablespace_name, bool &exists);

/**
 * @brief Check if a table exists in NDB Dictionary
 * @param dict          NDB Dictionary
 * @param db_name       Database name
 * @param table_name    Table name
 * @param exists [out]  Boolean that is set to true if the table exists, false
 *                      if not
 * @return true on success, false on failure
 */
bool ndb_table_exists(NdbDictionary::Dictionary *dict,
                      const std::string &db_name, const std::string &table_name,
                      bool &exists);

/**
 * @brief Retrieve the id and version of the logfile group definition in the NDB
 *        Dictionary
 * @param dict                 NDB Dictionary
 * @param logfile_group_name   Logfile group name
 * @param id [out]             Id of the logfile group
 * @param version [out]        Version of the logfile group
 * @return true on success, false on failure
 */
bool ndb_get_logfile_group_id_and_version(NdbDictionary::Dictionary *dict,
                                          const std::string &logfile_group_name,
                                          int &id, int &version);

/**
 * @brief Retrieve the id and version of the tablespace definition in the NDB
 *        Dictionary
 * @param dict              NDB Dictionary
 * @param tablespace_name   Tablespace name
 * @param id [out]          Id of the tablespace
 * @param version [out]     Version of the tablespace
 * @return true on success, false on failure
 */
bool ndb_get_tablespace_id_and_version(NdbDictionary::Dictionary *dict,
                                       const std::string &tablespace_name,
                                       int &id, int &version);

/**
 * @brief Return the number of indexes created on an NDB table
 * @param dict               NDB Dictionary
 * @param ndbtab             NDB Table object
 * @param index_count [out]  Number of indexes
 * @return true if the number of indexes could be determined, false if not
 */
bool ndb_table_index_count(const NdbDictionary::Dictionary *dict,
                           const NdbDictionary::Table *ndbtab,
                           unsigned int &index_count);
/**
 * @brief Scan the given table and delete the rows returned
 * @param ndb                    The Ndb Object
 * @param thd                    The THD object
 * @param ndb_table              NDB Table object whose rows are to be deleted
 * @param ndb_err [out]          NdbError object to send back the error during
 *                               a failure
 * @param ndb_scan_filter_defn   An optional std::function defining a
 *                               NdbScanFilter to be used by the scan
 * @return true if the rows were successfully deleted, false if not
 */
bool ndb_table_scan_and_delete_rows(
    Ndb *ndb, const THD *thd, const NdbDictionary::Table *ndb_table,
    NdbError &ndb_err,
    const std::function<void(NdbScanFilter &)> &ndb_scan_filter_defn = nullptr);

#endif
