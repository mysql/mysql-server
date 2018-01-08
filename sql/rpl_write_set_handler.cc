/* Copyright (c) 2014, 2017, Oracle and/or its affiliates. All rights reserved.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "sql/rpl_write_set_handler.h"

#include <assert.h>
#include <string.h>
#include <sys/types.h>
#include <map>
#include <string>
#include <vector>

#include "../extra/lz4/my_xxhash.h"  // IWYU pragma: keep
#include "lex_string.h"
#include "m_ctype.h"
#include "m_string.h"
#include "my_base.h"
#include "my_bitmap.h"
#include "my_dbug.h"
#include "my_inttypes.h"
#include "my_murmur3.h"    // murmur3_32
#include "my_stacktrace.h" // my_safe_itoa
#include "my_sys.h"
#include "mysql/service_mysql_alloc.h"
#include "mysql/udf_registration_types.h"
#include "sql/field.h"     // Field
#include "sql/handler.h"
#include "sql/key.h"
#include "sql/psi_memory_key.h"
#include "sql/query_options.h"
#include "sql/rpl_transaction_write_set_ctx.h"
#include "sql/session_tracker.h"
#include "sql/sql_class.h" // THD
#include "sql/sql_const.h"
#include "sql/sql_list.h"  // List
#include "sql/system_variables.h"
#include "sql/table.h"     // TABLE
#include "sql/transaction_info.h"
#include "sql_string.h"

#define NAME_READ_BUFFER_SIZE 1024
#define HASH_STRING_SEPARATOR "½"

const char *transaction_write_set_hashing_algorithms[]=
{
  "OFF",
  "MURMUR32",
  "XXHASH64"
  ,0
};

const char*
get_write_set_algorithm_string(unsigned int algorithm)
{
  switch(algorithm)
  {
    case HASH_ALGORITHM_OFF:
      return "OFF";
    case HASH_ALGORITHM_MURMUR32:
      return "MURMUR32";
    case HASH_ALGORITHM_XXHASH64:
      return "XXHASH64";
    default:
      return "UNKNOWN ALGORITHM";
  }
}

template <class type> uint64 calc_hash(ulong algorithm, type T, size_t len)
{
  if(algorithm == HASH_ALGORITHM_MURMUR32)
    return (murmur3_32((const uchar*)T, len, 0));
  else
    return (MY_XXH64((const uchar*)T, len, 0));
}

/**
  Function to check if the given TABLE has any foreign key field. This is
  needed to be checked to get the hash of the field value in the foreign
  table.

  @param[in] table - TABLE object
  @param[in] thd - THD object pointing to current thread.

  @param[out] foreign_key_map - a standard map which keeps track of the
                                foreign key fields.
*/
static void check_foreign_key(TABLE *table, THD *thd,
                              std::map<std::string,std::string> &foreign_key_map)
{
  DBUG_ENTER("check_foreign_key");
  /*
    OPTION_NO_FOREIGN_KEY_CHECKS bit in options_bits is set at two places

    1) If the user executed 'SET foreign_key_checks= 0' on the local session
    before executing the query.
    or
    2) We are applying a RBR event (i.e., the event is from a remote server)
    and logic in Rows_log_event::do_apply_event found out that the event is
    generated from a remote server session that disabled foreign_key_checks
    (using 'SET foreign_key_checks=0').

    In either of the above cases (i.e., the foreign key check is disabled for
    the current query/current event), we should ignore generating
    the foreign key information as they should not participate
    in the conflicts detecting algorithm.
  */
  if (thd->variables.option_bits & OPTION_NO_FOREIGN_KEY_CHECKS)
    DBUG_VOID_RETURN;

  if (0 == table->s->foreign_keys)
    DBUG_VOID_RETURN;

  TABLE_SHARE_FOREIGN_KEY_INFO *fk= table->s->foreign_key;
  std::string pke_prefix;
  pke_prefix.reserve(NAME_LEN * 5);

  for (uint i= 0; i < table->s->foreign_keys; i++)
  {
    /*
      There are two situations on which there is no
      unique_constraint_name, which means that the foreign key
      must be skipped.

      1) The referenced table was dropped using
         foreign_key_checks= 0, on that case we cannot check
         foreign key and need to skip it.

      2) The foreign key does reference a non unique key, thence
         it must be skipped since it cannot be used to check
         conflicts/dependencies.

         Example:
           CREATE TABLE t1 (c1 INT PRIMARY KEY, c2 INT, KEY(c2));
           CREATE TABLE t2 (x1 INT PRIMARY KEY, x2 INT,
                            FOREIGN KEY (x2) REFERENCES t1(c2));

           DELETE FROM t1 WHERE c1=1;
             does generate the PKEs:
               PRIMARY½test½4t1½21½1

           INSERT INTO t2 VALUES (1,1);
             does generate the PKEs:
               PRIMARY½test½4t2½21½1

           which does not contain PKE for the non unique key c2.
    */
    if (0 == fk[i].unique_constraint_name.length)
      continue;

    const std::string referenced_schema_name_length=
        std::to_string(fk[i].referenced_table_db.length);
    const std::string referenced_table_name_length=
        std::to_string(fk[i].referenced_table_name.length);

    /*
      Prefix the hash keys with the referenced index name.
    */
    pke_prefix.clear();
    pke_prefix.append(fk[i].unique_constraint_name.str,
                      fk[i].unique_constraint_name.length);
    pke_prefix.append(HASH_STRING_SEPARATOR);
    pke_prefix.append(fk[i].referenced_table_db.str,
                      fk[i].referenced_table_db.length);
    pke_prefix.append(HASH_STRING_SEPARATOR);
    pke_prefix.append(referenced_schema_name_length);
    pke_prefix.append(fk[i].referenced_table_name.str,
                      fk[i].referenced_table_name.length);
    pke_prefix.append(HASH_STRING_SEPARATOR);
    pke_prefix.append(referenced_table_name_length);

    /*
      Foreign key must not have a empty column list.
    */
    DBUG_ASSERT(fk[i].columns > 0);
    for (uint c= 0; c < fk[i].columns; c++)
      foreign_key_map[fk[i].column_name[c].str]= pke_prefix;
  }

  DBUG_VOID_RETURN;
}

#ifndef DBUG_OFF
static void debug_check_for_write_sets(std::vector<std::string> &key_list_to_hash)
{
  DBUG_EXECUTE_IF("PKE_assert_single_primary_key_generated_insert",
                  DBUG_ASSERT(key_list_to_hash.size() == 1);
                  DBUG_ASSERT(key_list_to_hash[0] == "PRIMARY" HASH_STRING_SEPARATOR "test" HASH_STRING_SEPARATOR "4t1"
                                                     HASH_STRING_SEPARATOR "21" HASH_STRING_SEPARATOR "1"););

  DBUG_EXECUTE_IF("PKE_assert_single_primary_key_generated_update",
                  DBUG_ASSERT(key_list_to_hash.size() == 1);
                  DBUG_ASSERT(key_list_to_hash[0] == "PRIMARY" HASH_STRING_SEPARATOR "test" HASH_STRING_SEPARATOR "4t1"
                                                     HASH_STRING_SEPARATOR "23" HASH_STRING_SEPARATOR "1" ||
                              key_list_to_hash[0] == "PRIMARY" HASH_STRING_SEPARATOR "test" HASH_STRING_SEPARATOR "4t1"
                                                     HASH_STRING_SEPARATOR "21" HASH_STRING_SEPARATOR "1"););

  DBUG_EXECUTE_IF("PKE_assert_multi_primary_key_generated_insert",
                  DBUG_ASSERT(key_list_to_hash.size() == 1);
                  DBUG_ASSERT(key_list_to_hash[0] == "PRIMARY" HASH_STRING_SEPARATOR "test" HASH_STRING_SEPARATOR "4t1"
                                                     HASH_STRING_SEPARATOR "21" HASH_STRING_SEPARATOR "12"
                                                     HASH_STRING_SEPARATOR "1"););

  DBUG_EXECUTE_IF("PKE_assert_multi_primary_key_generated_update",
                  DBUG_ASSERT(key_list_to_hash.size() == 1);
                  DBUG_ASSERT(key_list_to_hash[0] == "PRIMARY" HASH_STRING_SEPARATOR "test" HASH_STRING_SEPARATOR "4t1"
                                                     HASH_STRING_SEPARATOR "23" HASH_STRING_SEPARATOR "12"
                                                     HASH_STRING_SEPARATOR "1" ||
                              key_list_to_hash[0] == "PRIMARY" HASH_STRING_SEPARATOR "test" HASH_STRING_SEPARATOR "4t1"
                                                     HASH_STRING_SEPARATOR "21" HASH_STRING_SEPARATOR "12"
                                                     HASH_STRING_SEPARATOR "1"););

  DBUG_EXECUTE_IF("PKE_assert_single_primary_unique_key_generated_insert",
                  DBUG_ASSERT(key_list_to_hash.size() == 3);
                  DBUG_ASSERT(key_list_to_hash[0] == "PRIMARY" HASH_STRING_SEPARATOR "test" HASH_STRING_SEPARATOR "4t1"
                                                     HASH_STRING_SEPARATOR "21" HASH_STRING_SEPARATOR "1" &&
                              key_list_to_hash[1] == "c2" HASH_STRING_SEPARATOR "test" HASH_STRING_SEPARATOR "4t1"
                                                     HASH_STRING_SEPARATOR "22" HASH_STRING_SEPARATOR "1" &&
                              key_list_to_hash[2] == "c3" HASH_STRING_SEPARATOR "test" HASH_STRING_SEPARATOR "4t1"
                                                     HASH_STRING_SEPARATOR "23" HASH_STRING_SEPARATOR "1"););

  DBUG_EXECUTE_IF("PKE_assert_single_primary_unique_key_generated_update",
                  DBUG_ASSERT(key_list_to_hash.size() == 3);
                  DBUG_ASSERT((key_list_to_hash[0] == "PRIMARY" HASH_STRING_SEPARATOR "test" HASH_STRING_SEPARATOR "4t1"
                                                      HASH_STRING_SEPARATOR "25" HASH_STRING_SEPARATOR "1" &&
                               key_list_to_hash[1] == "c2" HASH_STRING_SEPARATOR "test" HASH_STRING_SEPARATOR "4t1"
                                                      HASH_STRING_SEPARATOR "22" HASH_STRING_SEPARATOR "1" &&
                               key_list_to_hash[2] == "c3" HASH_STRING_SEPARATOR "test" HASH_STRING_SEPARATOR "4t1"
                                                      HASH_STRING_SEPARATOR "23" HASH_STRING_SEPARATOR "1") ||
                              (key_list_to_hash[0] == "PRIMARY" HASH_STRING_SEPARATOR "test" HASH_STRING_SEPARATOR "4t1"
                                                      HASH_STRING_SEPARATOR "21" HASH_STRING_SEPARATOR "1" &&
                               key_list_to_hash[1] == "c2" HASH_STRING_SEPARATOR "test" HASH_STRING_SEPARATOR "4t1"
                                                      HASH_STRING_SEPARATOR "22" HASH_STRING_SEPARATOR "1" &&
                               key_list_to_hash[2] == "c3" HASH_STRING_SEPARATOR "test" HASH_STRING_SEPARATOR "4t1"
                                                      HASH_STRING_SEPARATOR "23" HASH_STRING_SEPARATOR "1")););

  DBUG_EXECUTE_IF("PKE_assert_multi_primary_unique_key_generated_insert",
                  DBUG_ASSERT(key_list_to_hash.size() == 3);
                  DBUG_ASSERT(key_list_to_hash[0] == "PRIMARY" HASH_STRING_SEPARATOR "test" HASH_STRING_SEPARATOR "4t1"
                                                     HASH_STRING_SEPARATOR "21" HASH_STRING_SEPARATOR "12"
                                                     HASH_STRING_SEPARATOR "1" &&
                              key_list_to_hash[1] == "b" HASH_STRING_SEPARATOR "test" HASH_STRING_SEPARATOR "4t1"
                                                     HASH_STRING_SEPARATOR "23" HASH_STRING_SEPARATOR "1" &&
                              key_list_to_hash[2] == "c" HASH_STRING_SEPARATOR "test" HASH_STRING_SEPARATOR "4t1"
                                                     HASH_STRING_SEPARATOR "24" HASH_STRING_SEPARATOR "1"););

  DBUG_EXECUTE_IF("PKE_assert_multi_primary_unique_key_generated_update",
                  DBUG_ASSERT(key_list_to_hash.size() == 3);
                  DBUG_ASSERT((key_list_to_hash[0] == "PRIMARY" HASH_STRING_SEPARATOR "test" HASH_STRING_SEPARATOR "4t1"
                                                       HASH_STRING_SEPARATOR "21" HASH_STRING_SEPARATOR "12"
                                                       HASH_STRING_SEPARATOR "1" &&
                               key_list_to_hash[1] == "b" HASH_STRING_SEPARATOR "test" HASH_STRING_SEPARATOR "4t1"
                                                       HASH_STRING_SEPARATOR "23" HASH_STRING_SEPARATOR "1" &&
                               key_list_to_hash[2] == "c" HASH_STRING_SEPARATOR "test" HASH_STRING_SEPARATOR "4t1"
                                                       HASH_STRING_SEPARATOR "24" HASH_STRING_SEPARATOR "1") ||
                              (key_list_to_hash[0] == "PRIMARY" HASH_STRING_SEPARATOR "test" HASH_STRING_SEPARATOR "4t1"
                                                       HASH_STRING_SEPARATOR "25" HASH_STRING_SEPARATOR "12"
                                                       HASH_STRING_SEPARATOR "1" &&
                               key_list_to_hash[1] == "b" HASH_STRING_SEPARATOR "test" HASH_STRING_SEPARATOR "4t1"
                                                       HASH_STRING_SEPARATOR "23" HASH_STRING_SEPARATOR "1" &&
                               key_list_to_hash[2] == "c" HASH_STRING_SEPARATOR "test" HASH_STRING_SEPARATOR "4t1"
                                                       HASH_STRING_SEPARATOR "24" HASH_STRING_SEPARATOR "1")););

  DBUG_EXECUTE_IF("PKE_assert_multi_foreign_key_generated_insert",
                  DBUG_ASSERT(key_list_to_hash.size() == 4);
                  DBUG_ASSERT(key_list_to_hash[0] == "PRIMARY" HASH_STRING_SEPARATOR "test" HASH_STRING_SEPARATOR "4t3"
                                                      HASH_STRING_SEPARATOR "21" HASH_STRING_SEPARATOR "15"
                                                      HASH_STRING_SEPARATOR "1" &&
                              key_list_to_hash[1] == "c2" HASH_STRING_SEPARATOR "test" HASH_STRING_SEPARATOR "4t3"
                                                      HASH_STRING_SEPARATOR "25" HASH_STRING_SEPARATOR "1" &&
                              key_list_to_hash[2] == "PRIMARY" HASH_STRING_SEPARATOR "test" HASH_STRING_SEPARATOR "4t1"
                                                      HASH_STRING_SEPARATOR "21" HASH_STRING_SEPARATOR "1" &&
                              key_list_to_hash[3] == "PRIMARY" HASH_STRING_SEPARATOR "test" HASH_STRING_SEPARATOR "4t2"
                                                      HASH_STRING_SEPARATOR "25" HASH_STRING_SEPARATOR "1"););

  DBUG_EXECUTE_IF("PKE_assert_multi_foreign_key_generated_update",
                  DBUG_ASSERT(key_list_to_hash.size() == 4);
                  DBUG_ASSERT((key_list_to_hash[0] == "PRIMARY" HASH_STRING_SEPARATOR "test" HASH_STRING_SEPARATOR "4t3"
                                                       HASH_STRING_SEPARATOR "21" HASH_STRING_SEPARATOR "15"
                                                       HASH_STRING_SEPARATOR "1" &&
                               key_list_to_hash[1] == "c2" HASH_STRING_SEPARATOR "test" HASH_STRING_SEPARATOR "4t3"
                                                       HASH_STRING_SEPARATOR "25" HASH_STRING_SEPARATOR "1" &&
                               key_list_to_hash[2] == "PRIMARY" HASH_STRING_SEPARATOR "test" HASH_STRING_SEPARATOR "4t1"
                                                       HASH_STRING_SEPARATOR "21" HASH_STRING_SEPARATOR "1" &&
                               key_list_to_hash[3] == "PRIMARY" HASH_STRING_SEPARATOR "test" HASH_STRING_SEPARATOR "4t2"
                                                       HASH_STRING_SEPARATOR "25" HASH_STRING_SEPARATOR "1") ||
                              (key_list_to_hash[0] == "PRIMARY" HASH_STRING_SEPARATOR "test" HASH_STRING_SEPARATOR "4t3"
                                                       HASH_STRING_SEPARATOR "23" HASH_STRING_SEPARATOR "15"
                                                       HASH_STRING_SEPARATOR "1" &&
                               key_list_to_hash[1] == "c2" HASH_STRING_SEPARATOR "test" HASH_STRING_SEPARATOR "4t3"
                                                       HASH_STRING_SEPARATOR "25" HASH_STRING_SEPARATOR "1" &&
                               key_list_to_hash[2] == "PRIMARY" HASH_STRING_SEPARATOR "test" HASH_STRING_SEPARATOR "4t1"
                                                       HASH_STRING_SEPARATOR "23" HASH_STRING_SEPARATOR "1" &&
                               key_list_to_hash[3] == "PRIMARY" HASH_STRING_SEPARATOR "test" HASH_STRING_SEPARATOR "4t2"
                                                       HASH_STRING_SEPARATOR "25" HASH_STRING_SEPARATOR "1")););

  DBUG_EXECUTE_IF("PKE_assert_foreign_key_on_referenced_unique_key_parent_generated_insert",
                  DBUG_ASSERT(key_list_to_hash.size() == 2);
                  DBUG_ASSERT(key_list_to_hash[0] == "PRIMARY" HASH_STRING_SEPARATOR "test" HASH_STRING_SEPARATOR "4t1"
                                                     HASH_STRING_SEPARATOR "22" HASH_STRING_SEPARATOR "1" &&
                              key_list_to_hash[1] == "c2" HASH_STRING_SEPARATOR "test" HASH_STRING_SEPARATOR "4t1"
                                                     HASH_STRING_SEPARATOR "22" HASH_STRING_SEPARATOR "1"););

  DBUG_EXECUTE_IF("PKE_assert_foreign_key_on_referenced_unique_key_generated_insert",
                  DBUG_ASSERT(key_list_to_hash.size() == 2);
                  DBUG_ASSERT(key_list_to_hash[0] == "PRIMARY" HASH_STRING_SEPARATOR "test" HASH_STRING_SEPARATOR "4t2"
                                                     HASH_STRING_SEPARATOR "21" HASH_STRING_SEPARATOR "1" &&
                              key_list_to_hash[1] == "c2" HASH_STRING_SEPARATOR "test" HASH_STRING_SEPARATOR "4t1"
                                                     HASH_STRING_SEPARATOR "21" HASH_STRING_SEPARATOR "1"););

  DBUG_EXECUTE_IF("PKE_assert_foreign_key_on_referenced_unique_key_generated_update",
                  DBUG_ASSERT(key_list_to_hash.size() == 2);
                  DBUG_ASSERT((key_list_to_hash[0] == "PRIMARY" HASH_STRING_SEPARATOR "test" HASH_STRING_SEPARATOR "4t2"
                                                      HASH_STRING_SEPARATOR "21" HASH_STRING_SEPARATOR "1" &&
                               key_list_to_hash[1] == "c2" HASH_STRING_SEPARATOR "test" HASH_STRING_SEPARATOR "4t1"
                                                      HASH_STRING_SEPARATOR "22" HASH_STRING_SEPARATOR "1") ||
                              (key_list_to_hash[0] == "PRIMARY" HASH_STRING_SEPARATOR "test" HASH_STRING_SEPARATOR "4t2"
                                                      HASH_STRING_SEPARATOR "21" HASH_STRING_SEPARATOR "1" &&
                               key_list_to_hash[1] == "c2" HASH_STRING_SEPARATOR "test" HASH_STRING_SEPARATOR "4t1"
                                                      HASH_STRING_SEPARATOR "21" HASH_STRING_SEPARATOR "1")););

  DBUG_EXECUTE_IF("PKE_assert_foreign_key_on_referenced_non_unique_key_parent_generated_insert",
                  DBUG_ASSERT(key_list_to_hash.size() == 1);
                  DBUG_ASSERT(key_list_to_hash[0] == "PRIMARY" HASH_STRING_SEPARATOR "test" HASH_STRING_SEPARATOR "4t1"
                                                     HASH_STRING_SEPARATOR "22" HASH_STRING_SEPARATOR "1"););

  DBUG_EXECUTE_IF("PKE_assert_foreign_key_on_referenced_non_unique_key_generated_insert",
                  DBUG_ASSERT(key_list_to_hash.size() == 1);
                  DBUG_ASSERT(key_list_to_hash[0] == "PRIMARY" HASH_STRING_SEPARATOR "test" HASH_STRING_SEPARATOR "4t2"
                                                     HASH_STRING_SEPARATOR "21" HASH_STRING_SEPARATOR "1"););

  DBUG_EXECUTE_IF("PKE_assert_foreign_key_on_referenced_non_unique_key_generated_update",
                  DBUG_ASSERT(key_list_to_hash.size() == 1);
                  DBUG_ASSERT(key_list_to_hash[0] == "PRIMARY" HASH_STRING_SEPARATOR "test" HASH_STRING_SEPARATOR "4t2"
                                                      HASH_STRING_SEPARATOR "21" HASH_STRING_SEPARATOR "1"););
}
#endif


/**
  Function to generate the hash of the string passed to this function.

  @param[in] pke - the string to be hashed.
  @param[in] thd - THD object pointing to current thread.
*/

static void generate_hash_pke(std::string pke, THD* thd)
{
  DBUG_ENTER("generate_hash_pke");
  DBUG_ASSERT(thd->variables.transaction_write_set_extraction !=
              HASH_ALGORITHM_OFF);
  const char* string_pke=NULL;
  string_pke= (char *)pke.c_str();
  DBUG_PRINT("info", ("The hashed value is %s for %u", string_pke,
                      thd->thread_id()));
  uint64 hash= calc_hash<const char *>(thd->variables.transaction_write_set_extraction,
                                       string_pke, pke.size());
  Rpl_transaction_write_set_ctx *transaction_write_set_ctc=
    thd->get_transaction()->get_transaction_write_set_ctx();
  transaction_write_set_ctc->add_write_set(hash);
  DBUG_VOID_RETURN;
}

void add_pke(TABLE *table, THD *thd)
{
  DBUG_ENTER("add_pke");
  std::string pke;
  std::string temporary_pke;

  // Fetching the foreign key value of the table and storing it in a map.
  std::map<std::string,std::string> foreign_key_map;
  check_foreign_key(table, thd, foreign_key_map);

  // The database name of the table in the transaction is fetched here.
  const char* database_name= table->s->db.str;
  uint length_database= strlen(database_name);
  char *buffer_db= (char*) my_malloc(
                                key_memory_write_set_extraction,
                                length_database, MYF(0));
  const char *char_length_database= my_safe_itoa(10, length_database,
                                                 &buffer_db[length_database-1]);
  temporary_pke.append(database_name);
  temporary_pke.append(HASH_STRING_SEPARATOR);
  temporary_pke.append(char_length_database);
  my_free(buffer_db);

  // The table name of the table in the transaction is fetched here.
  const char* table_name= table->s->table_name.str;
  uint length_table= strlen(table_name);
  char *buffer_table= (char*) my_malloc(
                                key_memory_write_set_extraction,
                                length_table, MYF(0));
  const char *char_length_table= my_safe_itoa(10, length_table, &buffer_table[length_table-1]);
  temporary_pke.append(table_name);
  temporary_pke.append(HASH_STRING_SEPARATOR);
  temporary_pke.append(char_length_table);

  my_free(buffer_table);

  // Finalizing the first part of the string to be hashed and storing it in
  // the pke.
  const char* temp_pke= NULL;
  temp_pke= (char *)temporary_pke.c_str();
  pke.append(temp_pke);

  /*
    The next section extracts the primary key equivalent of the rows that are
    changing during the current transaction.

    1. The primary key field is always stored in the key_part[0] so we can simply
       read the value from the table->s->keys.

    2. Along with primary key we also need to extract the unique key values to
       look for the places where we are breaking the unique key constraints.

    These keys (primary/unique) are prefixed with their index names.

    In MySQL, the name of a PRIMARY KEY is PRIMARY. For other indexes, if
    you do not assign a name, the index is assigned the same name as the
    first indexed column, with an optional suffix (_2, _3, ...) to make it
    unique.

    example :
       CREATE TABLE db1.t1 (i INT NOT NULL PRIMARY KEY, j INT UNIQUE KEY, k INT
                            UNIQUE KEY);

       INSERT INTO db1.t1 VALUES(1, 2, 3);

       Here the write set string will have three values and the prepared value before
       hash function is used will be :

       i -> PRIMARYdb13t1211 => PRIMARY is the index name (for primary key)

       j -> jdb13t1221       => 'j' is the index name (for first unique key)
       k -> kdb13t1231       => 'k' is the index name (for second unique key)

    Finally these value are hashed using the murmur hash function to prevent sending more
    for certification algorithm.
  */
  Rpl_transaction_write_set_ctx* ws_ctx=
    thd->get_transaction()->get_transaction_write_set_ctx();
  std::vector<std::string> key_list_to_hash;
  bitmap_set_all(table->read_set);
  int writeset_hashes_added= 0;

  if(table->key_info && (table->s->primary_key < MAX_KEY))
  {
    for (uint key_number=0; key_number < table->s->keys; key_number++)
    {
      // Skip non unique.
      if (!((table->key_info[key_number].flags & (HA_NOSAME )) == HA_NOSAME))
        continue;

      std::string unhashed_string;
      unhashed_string.append(table->key_info[key_number].name);
      unhashed_string.append(HASH_STRING_SEPARATOR);
      unhashed_string.append(pke);
      uint i= 0;
      for (/*empty*/; i < table->key_info[key_number].user_defined_key_parts; i++)
      {
        // read the primary key field values in str.
        int index= table->key_info[key_number].key_part[i].fieldnr;
        size_t length= 0;

        /* Ignore if the value is NULL. */
        if (table->field[index-1]->is_null())
          break;

        const CHARSET_INFO* cs= table->field[index-1]->charset();
        int max_length= cs->coll->strnxfrmlen(cs,
                                   table->field[index-1]->pack_length());

        char* pk_value= (char*) my_malloc(key_memory_write_set_extraction,
                                          max_length+1, MYF(MY_ZEROFILL));

        /*
          convert to normalized string and store so that it can be
          sorted using binary comparison functions like memcmp.
        */
        length= table->field[index-1]->make_sort_key((uchar*)pk_value,
                                                     max_length);
        pk_value[length]= 0;

        // buffer to be used for my_safe_itoa.
        char *buf= (char*) my_malloc(key_memory_write_set_extraction,
                                     length, MYF(0));
        const char *lenStr = my_safe_itoa(10, length, &buf[length-1]);

        unhashed_string.append(pk_value, length);
        unhashed_string.append(HASH_STRING_SEPARATOR);
        unhashed_string.append(lenStr);
        my_free(buf);
        my_free(pk_value);
      }
      /*
        If any part of the key is NULL, ignore adding it to hash keys.
        NULL cannot conflict with any value.
        Eg: create table t1(i int primary key not null, j int, k int,
                                                unique key (j, k));
            insert into t1 values (1, 2, NULL);
            insert into t1 values (2, 2, NULL); => this is allowed.
      */
      if (i == table->key_info[key_number].user_defined_key_parts)
      {
        key_list_to_hash.push_back(unhashed_string);
      }
      else
      {
        /* This is impossible to happen in case of primary keys */
        DBUG_ASSERT(key_number !=0);
      }
      unhashed_string.clear();
    }

    // This part takes care of the previously fetched foreign key values of
    // the referenced table adds it to the write set.
    for(uint i=0; i < table->s->fields; i++)
    {
      std::string referenced_FQTN=
        foreign_key_map[table->s->field[i]->field_name];
      if (referenced_FQTN.size() > 0)
      {
        size_t length= 0;

        /* Ignore if the value is NULL. */
        if (table->field[i]->is_null())
          continue;

        const CHARSET_INFO* cs= table->field[i]->charset();
        int max_length= cs->coll->strnxfrmlen(cs,
                                    table->field[i]->pack_length());

        char* pk_value= (char*) my_malloc(key_memory_write_set_extraction,
                                          max_length+1, MYF(MY_ZEROFILL));

        /*
          convert to normalized string and store so that it can be
          sorted using binary comparison functions like memcmp.
        */
        length= table->field[i]->make_sort_key((uchar*)pk_value, max_length);

        pk_value[length]= 0;

        // buffer to be used for my_safe_itoa.
        char *buf= (char*) my_malloc(key_memory_write_set_extraction,
                                     length, MYF(0));
        const char *lenStr = my_safe_itoa(10, length, &buf[length-1]);

        referenced_FQTN.append(pk_value, length);
        referenced_FQTN.append(HASH_STRING_SEPARATOR);
        referenced_FQTN.append(lenStr);

        my_free(buf);
        my_free(pk_value);
        key_list_to_hash.push_back(referenced_FQTN);
      }
    }

    if (table->file->referenced_by_foreign_key())
      ws_ctx->set_has_related_foreign_keys();

#ifndef DBUG_OFF
    debug_check_for_write_sets(key_list_to_hash);
#endif

    while(key_list_to_hash.size())
    {
      std::string prepared_string= key_list_to_hash.back();
      key_list_to_hash.pop_back();
      generate_hash_pke(prepared_string, thd);
      writeset_hashes_added++;
    }
  }

  if (writeset_hashes_added == 0)
    ws_ctx->set_has_missing_keys();

  DBUG_VOID_RETURN;
}
