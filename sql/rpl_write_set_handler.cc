/* Copyright (c) 2014, 2018, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "rpl_write_set_handler.h"

#include "my_global.h"
#include "my_stacktrace.h" // my_safe_itoa
#include "field.h"         // Field
#include "sql_class.h"     // THD
#include "sql_list.h"      // List
#include "table.h"         // TABLE
#include "rpl_handler.h"

#include "my_murmur3.h"    // murmur3_32
#include "../extra/lz4/my_xxhash.h" // xxHash

#include <map>
#include <string>
#include <utility>
#include <vector>

// The number of digits of ULLONG_MAX plus '\0'
#define VALUE_LENGTH_BUFFER_SIZE 24
#define NAME_READ_BUFFER_SIZE 1024
#define HASH_STRING_SEPARATOR "½"
#define COLLATION_CONVERSION_ALGORITHM 1

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

  This function is meant to be only called by add_pke() function, some
  conditions are check there for performance optimization.

  @param[in] table - TABLE object
  @param[in] thd - THD object pointing to current thread.

  @param[out] foreign_key_map - a standard map which keeps track of the
                                foreign key fields.
*/
static void check_foreign_key(TABLE *table, THD *thd,
                              std::map<std::string,std::string> &foreign_key_map)
{
  DBUG_ENTER("check_foreign_key");
  DBUG_ASSERT(!(thd->variables.option_bits & OPTION_NO_FOREIGN_KEY_CHECKS));
  char value_length_buffer[VALUE_LENGTH_BUFFER_SIZE];
  char* value_length= NULL;

  std::string pke_prefix;
  pke_prefix.reserve(NAME_LEN * 5);

  List<FOREIGN_KEY_INFO> f_key_list;
  table->file->get_foreign_key_list(thd, &f_key_list);

  FOREIGN_KEY_INFO *f_key_info;
  List_iterator_fast<FOREIGN_KEY_INFO> foreign_key_iterator(f_key_list);
  LEX_STRING *f_info;
  while ((f_key_info=foreign_key_iterator++))
  {
    /*
      If referenced_key_name is NULL it means that the parent table
      was dropped using foreign_key_checks= 0, on that case we
      cannot check foreign key and need to skip it.
    */
    if (f_key_info->referenced_key_name == NULL)
      continue;

    /*
      Prefix the hash keys with the referenced index name.
    */
    pke_prefix.clear();
    pke_prefix.append(f_key_info->referenced_key_name->str,
                      f_key_info->referenced_key_name->length);
    pke_prefix.append(HASH_STRING_SEPARATOR);
    pke_prefix.append(f_key_info->referenced_db->str,
                      f_key_info->referenced_db->length);
    pke_prefix.append(HASH_STRING_SEPARATOR);
    value_length= my_safe_itoa(10, f_key_info->referenced_db->length,
                               &value_length_buffer[VALUE_LENGTH_BUFFER_SIZE-1]);
    pke_prefix.append(value_length);
    pke_prefix.append(f_key_info->referenced_table->str,
                      f_key_info->referenced_table->length);
    pke_prefix.append(HASH_STRING_SEPARATOR);
    value_length= my_safe_itoa(10, f_key_info->referenced_table->length,
                               &value_length_buffer[VALUE_LENGTH_BUFFER_SIZE-1]);
    pke_prefix.append(value_length);

    List_iterator_fast<LEX_STRING> foreign_fields_iterator(f_key_info->foreign_fields);
    while ((f_info= foreign_fields_iterator++))
    {
      foreign_key_map[f_info->str]= pke_prefix;
    }
  }
  DBUG_VOID_RETURN;

}

#ifndef DBUG_OFF
static void debug_check_for_write_sets(std::vector<std::string> &key_list_to_hash)
{
  DBUG_EXECUTE_IF("PKE_assert_single_primary_key_generated_insert",
                  DBUG_ASSERT(key_list_to_hash.size() == 2);
                  DBUG_ASSERT(key_list_to_hash[1] == "PRIMARY" HASH_STRING_SEPARATOR "test" HASH_STRING_SEPARATOR "4t1"
                                                     HASH_STRING_SEPARATOR "21" HASH_STRING_SEPARATOR "1"););

  DBUG_EXECUTE_IF("PKE_assert_single_primary_key_generated_insert_collation",
                  DBUG_ASSERT(key_list_to_hash.size() == 2);
                  DBUG_ASSERT(key_list_to_hash[0] == "PRIMARY" HASH_STRING_SEPARATOR "test" HASH_STRING_SEPARATOR "4t1"
                                                     HASH_STRING_SEPARATOR "21" HASH_STRING_SEPARATOR "1" &&
                              key_list_to_hash[1] == key_list_to_hash[0]););

  DBUG_EXECUTE_IF("PKE_assert_single_primary_key_generated_update",
                  DBUG_ASSERT(key_list_to_hash.size() == 2);
                  DBUG_ASSERT(key_list_to_hash[1] == "PRIMARY" HASH_STRING_SEPARATOR "test" HASH_STRING_SEPARATOR "4t1"
                                                     HASH_STRING_SEPARATOR "23" HASH_STRING_SEPARATOR "1" ||
                              key_list_to_hash[1] == "PRIMARY" HASH_STRING_SEPARATOR "test" HASH_STRING_SEPARATOR "4t1"
                                                     HASH_STRING_SEPARATOR "21" HASH_STRING_SEPARATOR "1"););

  DBUG_EXECUTE_IF("PKE_assert_single_primary_key_generated_update_collation",
                  DBUG_ASSERT(key_list_to_hash.size() == 2);
                  DBUG_ASSERT((key_list_to_hash[0] == "PRIMARY" HASH_STRING_SEPARATOR "test" HASH_STRING_SEPARATOR "4t1"
                                                     HASH_STRING_SEPARATOR "23" HASH_STRING_SEPARATOR "1" &&
                               key_list_to_hash[1] == key_list_to_hash[0]) ||
                              (key_list_to_hash[0] == "PRIMARY" HASH_STRING_SEPARATOR "test" HASH_STRING_SEPARATOR "4t1"
                                                     HASH_STRING_SEPARATOR "21" HASH_STRING_SEPARATOR "1" &&
                               key_list_to_hash[1] == key_list_to_hash[0])););

  DBUG_EXECUTE_IF("PKE_assert_multi_primary_key_generated_insert",
                  DBUG_ASSERT(key_list_to_hash.size() == 2);
                  DBUG_ASSERT(key_list_to_hash[1] == "PRIMARY" HASH_STRING_SEPARATOR "test" HASH_STRING_SEPARATOR "4t1"
                                                     HASH_STRING_SEPARATOR "21" HASH_STRING_SEPARATOR "12"
                                                     HASH_STRING_SEPARATOR "1"););

  DBUG_EXECUTE_IF("PKE_assert_multi_primary_key_generated_insert_collation",
                  DBUG_ASSERT(key_list_to_hash.size() == 2);
                  DBUG_ASSERT(key_list_to_hash[0] == "PRIMARY" HASH_STRING_SEPARATOR "test" HASH_STRING_SEPARATOR "4t1"
                                                     HASH_STRING_SEPARATOR "21" HASH_STRING_SEPARATOR "12"
                                                     HASH_STRING_SEPARATOR "1" &&
                              key_list_to_hash[1] == key_list_to_hash[0]););

  DBUG_EXECUTE_IF("PKE_assert_multi_primary_key_generated_update",
                  DBUG_ASSERT(key_list_to_hash.size() == 2);
                  DBUG_ASSERT(key_list_to_hash[1] == "PRIMARY" HASH_STRING_SEPARATOR "test" HASH_STRING_SEPARATOR "4t1"
                                                     HASH_STRING_SEPARATOR "23" HASH_STRING_SEPARATOR "12"
                                                     HASH_STRING_SEPARATOR "1" ||
                              key_list_to_hash[1] == "PRIMARY" HASH_STRING_SEPARATOR "test" HASH_STRING_SEPARATOR "4t1"
                                                     HASH_STRING_SEPARATOR "21" HASH_STRING_SEPARATOR "12"
                                                     HASH_STRING_SEPARATOR "1"););

  DBUG_EXECUTE_IF("PKE_assert_multi_primary_key_generated_update_collation",
                  DBUG_ASSERT(key_list_to_hash.size() == 2);
                  DBUG_ASSERT((key_list_to_hash[0] == "PRIMARY" HASH_STRING_SEPARATOR "test" HASH_STRING_SEPARATOR "4t1"
                                                     HASH_STRING_SEPARATOR "23" HASH_STRING_SEPARATOR "12"
                                                     HASH_STRING_SEPARATOR "1" &&
                               key_list_to_hash[1] == key_list_to_hash[0]) ||
                              (key_list_to_hash[0] == "PRIMARY" HASH_STRING_SEPARATOR "test" HASH_STRING_SEPARATOR "4t1"
                                                     HASH_STRING_SEPARATOR "21" HASH_STRING_SEPARATOR "12"
                                                     HASH_STRING_SEPARATOR "1" &&
                               key_list_to_hash[1] == key_list_to_hash[0])););

  DBUG_EXECUTE_IF("PKE_assert_single_primary_unique_key_generated_insert",
                  DBUG_ASSERT(key_list_to_hash.size() == 6);
                  DBUG_ASSERT(key_list_to_hash[1] == "PRIMARY" HASH_STRING_SEPARATOR "test" HASH_STRING_SEPARATOR "4t1"
                                                     HASH_STRING_SEPARATOR "21" HASH_STRING_SEPARATOR "1" &&
                              key_list_to_hash[3] == "c2" HASH_STRING_SEPARATOR "test" HASH_STRING_SEPARATOR "4t1"
                                                     HASH_STRING_SEPARATOR "22" HASH_STRING_SEPARATOR "1" &&
                              key_list_to_hash[5] == "c3" HASH_STRING_SEPARATOR "test" HASH_STRING_SEPARATOR "4t1"
                                                     HASH_STRING_SEPARATOR "23" HASH_STRING_SEPARATOR "1"););


  DBUG_EXECUTE_IF("PKE_assert_single_primary_unique_key_generated_insert_collation",
                  DBUG_ASSERT(key_list_to_hash.size() == 6);
                  DBUG_ASSERT(key_list_to_hash[0] == "PRIMARY" HASH_STRING_SEPARATOR "test" HASH_STRING_SEPARATOR "4t1"
                                                     HASH_STRING_SEPARATOR "21" HASH_STRING_SEPARATOR "1" &&
                              key_list_to_hash[2] == "c2" HASH_STRING_SEPARATOR "test" HASH_STRING_SEPARATOR "4t1"
                                                     HASH_STRING_SEPARATOR "22" HASH_STRING_SEPARATOR "1" &&
                              key_list_to_hash[4] == "c3" HASH_STRING_SEPARATOR "test" HASH_STRING_SEPARATOR "4t1"
                                                     HASH_STRING_SEPARATOR "23" HASH_STRING_SEPARATOR "1" &&
                              key_list_to_hash[1] == key_list_to_hash[0] &&
                              key_list_to_hash[3] == key_list_to_hash[2] &&
                              key_list_to_hash[5] == key_list_to_hash[4]););

  DBUG_EXECUTE_IF("PKE_assert_single_primary_unique_key_generated_update",
                  DBUG_ASSERT(key_list_to_hash.size() == 6);
                  DBUG_ASSERT((key_list_to_hash[1] == "PRIMARY" HASH_STRING_SEPARATOR "test" HASH_STRING_SEPARATOR "4t1"
                                                      HASH_STRING_SEPARATOR "25" HASH_STRING_SEPARATOR "1" &&
                               key_list_to_hash[3] == "c2" HASH_STRING_SEPARATOR "test" HASH_STRING_SEPARATOR "4t1"
                                                      HASH_STRING_SEPARATOR "22" HASH_STRING_SEPARATOR "1" &&
                               key_list_to_hash[5] == "c3" HASH_STRING_SEPARATOR "test" HASH_STRING_SEPARATOR "4t1"
                                                      HASH_STRING_SEPARATOR "23" HASH_STRING_SEPARATOR "1") ||
                              (key_list_to_hash[1] == "PRIMARY" HASH_STRING_SEPARATOR "test" HASH_STRING_SEPARATOR "4t1"
                                                      HASH_STRING_SEPARATOR "21" HASH_STRING_SEPARATOR "1" &&
                               key_list_to_hash[3] == "c2" HASH_STRING_SEPARATOR "test" HASH_STRING_SEPARATOR "4t1"
                                                      HASH_STRING_SEPARATOR "22" HASH_STRING_SEPARATOR "1" &&
                               key_list_to_hash[5] == "c3" HASH_STRING_SEPARATOR "test" HASH_STRING_SEPARATOR "4t1"
                                                      HASH_STRING_SEPARATOR "23" HASH_STRING_SEPARATOR "1")););

  DBUG_EXECUTE_IF("PKE_assert_single_primary_unique_key_generated_update_collation",
                  DBUG_ASSERT(key_list_to_hash.size() == 6);
                  DBUG_ASSERT((key_list_to_hash[0] == "PRIMARY" HASH_STRING_SEPARATOR "test" HASH_STRING_SEPARATOR "4t1"
                                                      HASH_STRING_SEPARATOR "25" HASH_STRING_SEPARATOR "1" &&
                               key_list_to_hash[2] == "c2" HASH_STRING_SEPARATOR "test" HASH_STRING_SEPARATOR "4t1"
                                                      HASH_STRING_SEPARATOR "22" HASH_STRING_SEPARATOR "1" &&
                               key_list_to_hash[4] == "c3" HASH_STRING_SEPARATOR "test" HASH_STRING_SEPARATOR "4t1"
                                                      HASH_STRING_SEPARATOR "23" HASH_STRING_SEPARATOR "1" &&
                               key_list_to_hash[1] == key_list_to_hash[0] &&
                               key_list_to_hash[3] == key_list_to_hash[2] &&
                               key_list_to_hash[5] == key_list_to_hash[4]) ||
                              (key_list_to_hash[0] == "PRIMARY" HASH_STRING_SEPARATOR "test" HASH_STRING_SEPARATOR "4t1"
                                                      HASH_STRING_SEPARATOR "21" HASH_STRING_SEPARATOR "1" &&
                               key_list_to_hash[2] == "c2" HASH_STRING_SEPARATOR "test" HASH_STRING_SEPARATOR "4t1"
                                                      HASH_STRING_SEPARATOR "22" HASH_STRING_SEPARATOR "1" &&
                               key_list_to_hash[4] == "c3" HASH_STRING_SEPARATOR "test" HASH_STRING_SEPARATOR "4t1"
                                                      HASH_STRING_SEPARATOR "23" HASH_STRING_SEPARATOR "1" &&
                               key_list_to_hash[1] == key_list_to_hash[0] &&
                               key_list_to_hash[3] == key_list_to_hash[2] &&
                               key_list_to_hash[5] == key_list_to_hash[4])););

  DBUG_EXECUTE_IF("PKE_assert_multi_primary_unique_key_generated_insert",
                  DBUG_ASSERT(key_list_to_hash.size() == 6);
                  DBUG_ASSERT(key_list_to_hash[1] == "PRIMARY" HASH_STRING_SEPARATOR "test" HASH_STRING_SEPARATOR "4t1"
                                                     HASH_STRING_SEPARATOR "21" HASH_STRING_SEPARATOR "12"
                                                     HASH_STRING_SEPARATOR "1" &&
                              key_list_to_hash[3] == "b" HASH_STRING_SEPARATOR "test" HASH_STRING_SEPARATOR "4t1"
                                                     HASH_STRING_SEPARATOR "23" HASH_STRING_SEPARATOR "1" &&
                              key_list_to_hash[5] == "c" HASH_STRING_SEPARATOR "test" HASH_STRING_SEPARATOR "4t1"
                                                     HASH_STRING_SEPARATOR "24" HASH_STRING_SEPARATOR "1"););

  DBUG_EXECUTE_IF("PKE_assert_multi_primary_unique_key_generated_insert_collation",
                  DBUG_ASSERT(key_list_to_hash.size() == 6);
                  DBUG_ASSERT(key_list_to_hash[0] == "PRIMARY" HASH_STRING_SEPARATOR "test" HASH_STRING_SEPARATOR "4t1"
                                                     HASH_STRING_SEPARATOR "21" HASH_STRING_SEPARATOR "12"
                                                     HASH_STRING_SEPARATOR "1" &&
                              key_list_to_hash[2] == "b" HASH_STRING_SEPARATOR "test" HASH_STRING_SEPARATOR "4t1"
                                                     HASH_STRING_SEPARATOR "23" HASH_STRING_SEPARATOR "1" &&
                              key_list_to_hash[4] == "c" HASH_STRING_SEPARATOR "test" HASH_STRING_SEPARATOR "4t1"
                                                     HASH_STRING_SEPARATOR "24" HASH_STRING_SEPARATOR "1" &&
                              key_list_to_hash[1] == key_list_to_hash[0] &&
                              key_list_to_hash[3] == key_list_to_hash[2] &&
                              key_list_to_hash[5] == key_list_to_hash[4]););

  DBUG_EXECUTE_IF("PKE_assert_multi_primary_unique_key_generated_update",
                  DBUG_ASSERT(key_list_to_hash.size() == 6);
                  DBUG_ASSERT((key_list_to_hash[1] == "PRIMARY" HASH_STRING_SEPARATOR "test" HASH_STRING_SEPARATOR "4t1"
                                                       HASH_STRING_SEPARATOR "21" HASH_STRING_SEPARATOR "12"
                                                       HASH_STRING_SEPARATOR "1" &&
                               key_list_to_hash[3] == "b" HASH_STRING_SEPARATOR "test" HASH_STRING_SEPARATOR "4t1"
                                                       HASH_STRING_SEPARATOR "23" HASH_STRING_SEPARATOR "1" &&
                               key_list_to_hash[5] == "c" HASH_STRING_SEPARATOR "test" HASH_STRING_SEPARATOR "4t1"
                                                       HASH_STRING_SEPARATOR "24" HASH_STRING_SEPARATOR "1") ||
                              (key_list_to_hash[1] == "PRIMARY" HASH_STRING_SEPARATOR "test" HASH_STRING_SEPARATOR "4t1"
                                                       HASH_STRING_SEPARATOR "25" HASH_STRING_SEPARATOR "12"
                                                       HASH_STRING_SEPARATOR "1" &&
                               key_list_to_hash[3] == "b" HASH_STRING_SEPARATOR "test" HASH_STRING_SEPARATOR "4t1"
                                                       HASH_STRING_SEPARATOR "23" HASH_STRING_SEPARATOR "1" &&
                               key_list_to_hash[5] == "c" HASH_STRING_SEPARATOR "test" HASH_STRING_SEPARATOR "4t1"
                                                       HASH_STRING_SEPARATOR "24" HASH_STRING_SEPARATOR "1")););

  DBUG_EXECUTE_IF("PKE_assert_multi_primary_unique_key_generated_update_collation",
                  DBUG_ASSERT(key_list_to_hash.size() == 6);
                  DBUG_ASSERT((key_list_to_hash[0] == "PRIMARY" HASH_STRING_SEPARATOR "test" HASH_STRING_SEPARATOR "4t1"
                                                       HASH_STRING_SEPARATOR "21" HASH_STRING_SEPARATOR "12"
                                                       HASH_STRING_SEPARATOR "1" &&
                               key_list_to_hash[2] == "b" HASH_STRING_SEPARATOR "test" HASH_STRING_SEPARATOR "4t1"
                                                       HASH_STRING_SEPARATOR "23" HASH_STRING_SEPARATOR "1" &&
                               key_list_to_hash[4] == "c" HASH_STRING_SEPARATOR "test" HASH_STRING_SEPARATOR "4t1"
                                                       HASH_STRING_SEPARATOR "24" HASH_STRING_SEPARATOR "1" &&
                               key_list_to_hash[1] == key_list_to_hash[0] &&
                               key_list_to_hash[3] == key_list_to_hash[2] &&
                               key_list_to_hash[5] == key_list_to_hash[4]) ||
                              (key_list_to_hash[0] == "PRIMARY" HASH_STRING_SEPARATOR "test" HASH_STRING_SEPARATOR "4t1"
                                                       HASH_STRING_SEPARATOR "25" HASH_STRING_SEPARATOR "12"
                                                       HASH_STRING_SEPARATOR "1" &&
                               key_list_to_hash[2] == "b" HASH_STRING_SEPARATOR "test" HASH_STRING_SEPARATOR "4t1"
                                                       HASH_STRING_SEPARATOR "23" HASH_STRING_SEPARATOR "1" &&
                               key_list_to_hash[4] == "c" HASH_STRING_SEPARATOR "test" HASH_STRING_SEPARATOR "4t1"
                                                       HASH_STRING_SEPARATOR "24" HASH_STRING_SEPARATOR "1" &&
                               key_list_to_hash[1] == key_list_to_hash[0] &&
                               key_list_to_hash[3] == key_list_to_hash[2] &&
                               key_list_to_hash[5] == key_list_to_hash[4])););

  DBUG_EXECUTE_IF("PKE_assert_multi_foreign_key_generated_insert",
                  DBUG_ASSERT(key_list_to_hash.size() == 8);
                  DBUG_ASSERT(key_list_to_hash[1] == "PRIMARY" HASH_STRING_SEPARATOR "test" HASH_STRING_SEPARATOR "4t3"
                                                      HASH_STRING_SEPARATOR "21" HASH_STRING_SEPARATOR "15"
                                                      HASH_STRING_SEPARATOR "1" &&
                              key_list_to_hash[3] == "c2" HASH_STRING_SEPARATOR "test" HASH_STRING_SEPARATOR "4t3"
                                                      HASH_STRING_SEPARATOR "25" HASH_STRING_SEPARATOR "1" &&
                              key_list_to_hash[5] == "PRIMARY" HASH_STRING_SEPARATOR "test" HASH_STRING_SEPARATOR "4t1"
                                                      HASH_STRING_SEPARATOR "21" HASH_STRING_SEPARATOR "1" &&
                              key_list_to_hash[7] == "PRIMARY" HASH_STRING_SEPARATOR "test" HASH_STRING_SEPARATOR "4t2"
                                                      HASH_STRING_SEPARATOR "25" HASH_STRING_SEPARATOR "1"););

  DBUG_EXECUTE_IF("PKE_assert_multi_foreign_key_generated_insert_collation",
                  DBUG_ASSERT(key_list_to_hash.size() == 8);
                  DBUG_ASSERT(key_list_to_hash[0] == "PRIMARY" HASH_STRING_SEPARATOR "test" HASH_STRING_SEPARATOR "4t3"
                                                      HASH_STRING_SEPARATOR "21" HASH_STRING_SEPARATOR "15"
                                                      HASH_STRING_SEPARATOR "1" &&
                              key_list_to_hash[2] == "c2" HASH_STRING_SEPARATOR "test" HASH_STRING_SEPARATOR "4t3"
                                                      HASH_STRING_SEPARATOR "25" HASH_STRING_SEPARATOR "1" &&
                              key_list_to_hash[4] == "PRIMARY" HASH_STRING_SEPARATOR "test" HASH_STRING_SEPARATOR "4t1"
                                                      HASH_STRING_SEPARATOR "21" HASH_STRING_SEPARATOR "1" &&
                              key_list_to_hash[6] == "PRIMARY" HASH_STRING_SEPARATOR "test" HASH_STRING_SEPARATOR "4t2"
                                                      HASH_STRING_SEPARATOR "25" HASH_STRING_SEPARATOR "1" &&
                              key_list_to_hash[1] == key_list_to_hash[0] &&
                              key_list_to_hash[3] == key_list_to_hash[2] &&
                              key_list_to_hash[5] == key_list_to_hash[4] &&
                              key_list_to_hash[7] == key_list_to_hash[6]););

  DBUG_EXECUTE_IF("PKE_assert_multi_foreign_key_generated_update",
                  DBUG_ASSERT(key_list_to_hash.size() == 8);
                  DBUG_ASSERT((key_list_to_hash[1] == "PRIMARY" HASH_STRING_SEPARATOR "test" HASH_STRING_SEPARATOR "4t3"
                                                       HASH_STRING_SEPARATOR "21" HASH_STRING_SEPARATOR "15"
                                                       HASH_STRING_SEPARATOR "1" &&
                               key_list_to_hash[3] == "c2" HASH_STRING_SEPARATOR "test" HASH_STRING_SEPARATOR "4t3"
                                                       HASH_STRING_SEPARATOR "25" HASH_STRING_SEPARATOR "1" &&
                               key_list_to_hash[5] == "PRIMARY" HASH_STRING_SEPARATOR "test" HASH_STRING_SEPARATOR "4t1"
                                                       HASH_STRING_SEPARATOR "21" HASH_STRING_SEPARATOR "1" &&
                               key_list_to_hash[7] == "PRIMARY" HASH_STRING_SEPARATOR "test" HASH_STRING_SEPARATOR "4t2"
                                                       HASH_STRING_SEPARATOR "25" HASH_STRING_SEPARATOR "1") ||
                              (key_list_to_hash[1] == "PRIMARY" HASH_STRING_SEPARATOR "test" HASH_STRING_SEPARATOR "4t3"
                                                       HASH_STRING_SEPARATOR "23" HASH_STRING_SEPARATOR "15"
                                                       HASH_STRING_SEPARATOR "1" &&
                               key_list_to_hash[3] == "c2" HASH_STRING_SEPARATOR "test" HASH_STRING_SEPARATOR "4t3"
                                                       HASH_STRING_SEPARATOR "25" HASH_STRING_SEPARATOR "1" &&
                               key_list_to_hash[5] == "PRIMARY" HASH_STRING_SEPARATOR "test" HASH_STRING_SEPARATOR "4t1"
                                                       HASH_STRING_SEPARATOR "23" HASH_STRING_SEPARATOR "1" &&
                               key_list_to_hash[7] == "PRIMARY" HASH_STRING_SEPARATOR "test" HASH_STRING_SEPARATOR "4t2"
                                                       HASH_STRING_SEPARATOR "25" HASH_STRING_SEPARATOR "1")););

  DBUG_EXECUTE_IF("PKE_assert_multi_foreign_key_generated_update_collation",
                  DBUG_ASSERT(key_list_to_hash.size() == 8);
                  DBUG_ASSERT((key_list_to_hash[0] == "PRIMARY" HASH_STRING_SEPARATOR "test" HASH_STRING_SEPARATOR "4t3"
                                                       HASH_STRING_SEPARATOR "21" HASH_STRING_SEPARATOR "15"
                                                       HASH_STRING_SEPARATOR "1" &&
                               key_list_to_hash[2] == "c2" HASH_STRING_SEPARATOR "test" HASH_STRING_SEPARATOR "4t3"
                                                       HASH_STRING_SEPARATOR "25" HASH_STRING_SEPARATOR "1" &&
                               key_list_to_hash[4] == "PRIMARY" HASH_STRING_SEPARATOR "test" HASH_STRING_SEPARATOR "4t1"
                                                       HASH_STRING_SEPARATOR "21" HASH_STRING_SEPARATOR "1" &&
                               key_list_to_hash[6] == "PRIMARY" HASH_STRING_SEPARATOR "test" HASH_STRING_SEPARATOR "4t2"
                                                       HASH_STRING_SEPARATOR "25" HASH_STRING_SEPARATOR "1" &&
                               key_list_to_hash[1] == key_list_to_hash[0] &&
                               key_list_to_hash[3] == key_list_to_hash[2] &&
                               key_list_to_hash[4] == key_list_to_hash[4] &&
                               key_list_to_hash[7] == key_list_to_hash[6]) ||
                              (key_list_to_hash[0] == "PRIMARY" HASH_STRING_SEPARATOR "test" HASH_STRING_SEPARATOR "4t3"
                                                       HASH_STRING_SEPARATOR "23" HASH_STRING_SEPARATOR "15"
                                                       HASH_STRING_SEPARATOR "1" &&
                               key_list_to_hash[2] == "c2" HASH_STRING_SEPARATOR "test" HASH_STRING_SEPARATOR "4t3"
                                                       HASH_STRING_SEPARATOR "25" HASH_STRING_SEPARATOR "1" &&
                               key_list_to_hash[4] == "PRIMARY" HASH_STRING_SEPARATOR "test" HASH_STRING_SEPARATOR "4t1"
                                                       HASH_STRING_SEPARATOR "23" HASH_STRING_SEPARATOR "1" &&
                               key_list_to_hash[6] == "PRIMARY" HASH_STRING_SEPARATOR "test" HASH_STRING_SEPARATOR "4t2"
                                                       HASH_STRING_SEPARATOR "25" HASH_STRING_SEPARATOR "1" &&
                               key_list_to_hash[1] == key_list_to_hash[0] &&
                               key_list_to_hash[3] == key_list_to_hash[2] &&
                               key_list_to_hash[4] == key_list_to_hash[4] &&
                               key_list_to_hash[7] == key_list_to_hash[6])););
}
#endif


/**
  Function to generate the hash of the string passed to this function.

  @param[in] pke - the string to be hashed.
  @param[in] collation_conversion_algorithm - algorithm used for the conversion
                                              0 - converted using without
                                                  collation support algorithm
                                              1 - converted using with collation
                                                  support conversion algorithm

  @param[in] thd - THD object pointing to current thread.
*/

static void generate_hash_pke(const std::string &pke, uint collation_conversion_algorithm, THD* thd)
{
  DBUG_ENTER("generate_hash_pke");
  DBUG_ASSERT(thd->variables.transaction_write_set_extraction !=
              HASH_ALGORITHM_OFF);

  size_t length= (COLLATION_CONVERSION_ALGORITHM == collation_conversion_algorithm) ?
                   pke.size() : strlen(pke.c_str());
  uint64 hash= calc_hash<const char *>(thd->variables.transaction_write_set_extraction,
                                       pke.c_str(), length);
  thd->get_transaction()->get_transaction_write_set_ctx()->add_write_set(hash);

  DBUG_PRINT("info", ("pke: %s; hash: %llu", pke.c_str(), hash));
  DBUG_VOID_RETURN;
}


void add_pke(TABLE *table, THD *thd)
{
  DBUG_ENTER("add_pke");
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
  int writeset_hashes_added= 0;

  if(table->key_info && (table->s->primary_key < MAX_KEY))
  {
    char value_length_buffer[VALUE_LENGTH_BUFFER_SIZE];
    char* value_length= NULL;

    std::string pke_schema_table;
    pke_schema_table.reserve(NAME_LEN * 3);
    pke_schema_table.append(HASH_STRING_SEPARATOR);
    pke_schema_table.append(table->s->db.str, table->s->db.length);
    pke_schema_table.append(HASH_STRING_SEPARATOR);
    value_length= my_safe_itoa(10, table->s->db.length,
                               &value_length_buffer[VALUE_LENGTH_BUFFER_SIZE-1]);
    pke_schema_table.append(value_length);
    pke_schema_table.append(table->s->table_name.str, table->s->table_name.length);
    pke_schema_table.append(HASH_STRING_SEPARATOR);
    value_length= my_safe_itoa(10, table->s->table_name.length,
                               &value_length_buffer[VALUE_LENGTH_BUFFER_SIZE-1]);
    pke_schema_table.append(value_length);

    std::string pke;
    pke.reserve(NAME_LEN * 5);

    char *pk_value= NULL;
    size_t pk_value_size= 0;

    // Buffer to read the names of the database and table names which is less
    // than 1024. So its a safe limit.
    char name_read_buffer[NAME_READ_BUFFER_SIZE];
    // Buffer to read the row data from the table record[0].
    String row_data(name_read_buffer, sizeof(name_read_buffer), &my_charset_bin);

#ifndef DBUG_OFF
    std::vector<std::string> write_sets;
#endif

    for (uint key_number=0; key_number < table->s->keys; key_number++)
    {
      // Skip non unique.
      if (!((table->key_info[key_number].flags & (HA_NOSAME )) == HA_NOSAME))
        continue;

      /*
        To handle both members having hash values with and without collation
        in the same group, we generate and send both versions (with and without
        collation) of the hash in the newer versions. This would mean that a row
        change will generate 2 instead of 1 writeset, and 4 instead of 2, when PK
        are involved. This will mean that a transaction will be certified against
        two writesets instead of just one.

        To generate both versions (with and without collation) of the hash, it
        first converts using without collation support algorithm (old algorithm),
        and then using with collation support conversion algorithm, and adds
        generated value to key_list_to_hash vector, for hash generation later.

        Since the collation writeset is bigger or equal than the raw one, we do
        generate first the collation and reuse the buffer without the need to
        resize for the raw.
      */
      for (int collation_conversion_algorithm= COLLATION_CONVERSION_ALGORITHM;
           collation_conversion_algorithm >= 0;
           collation_conversion_algorithm--)
      {
        pke.clear();
        pke.append(table->key_info[key_number].name);
        pke.append(pke_schema_table);

        uint i= 0;
        for (/*empty*/; i < table->key_info[key_number].user_defined_key_parts; i++)
        {
          // read the primary key field values in str.
          int index= table->key_info[key_number].key_part[i].fieldnr;
          size_t length= 0;

          /* Ignore if the value is NULL. */
          if (table->field[index-1]->is_null())
            break;

          // convert using collation support conversion algorithm
          if (COLLATION_CONVERSION_ALGORITHM == collation_conversion_algorithm)
          {
            const CHARSET_INFO* cs= table->field[index-1]->charset();
            length= cs->coll->strnxfrmlen(cs,
                                       table->field[index-1]->pack_length());
          }
          // convert using without collation support algorithm
          else
          {
            table->field[index-1]->val_str(&row_data);
            length= row_data.length();
          }

          if (pk_value_size < length+1)
          {
            pk_value_size= length+1;
            pk_value= (char*) my_realloc(key_memory_write_set_extraction,
                                         pk_value, pk_value_size,
                                         MYF(MY_ZEROFILL));
          }

          // convert using collation support conversion algorithm
          if (COLLATION_CONVERSION_ALGORITHM == collation_conversion_algorithm)
          {
            /*
              convert to normalized string and store so that it can be
              sorted using binary comparison functions like memcmp.
            */
            table->field[index-1]->make_sort_key((uchar*)pk_value, length);
            pk_value[length]= 0;
          }
          // convert using without collation support algorithm
          else
          {
            strmake(pk_value, row_data.c_ptr_safe(), length);
          }

          pke.append(pk_value, length);
          pke.append(HASH_STRING_SEPARATOR);
          value_length= my_safe_itoa(10, length,
                                     &value_length_buffer[VALUE_LENGTH_BUFFER_SIZE-1]);
          pke.append(value_length);
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
          generate_hash_pke(pke, collation_conversion_algorithm, thd);
          writeset_hashes_added++;
#ifndef DBUG_OFF
          write_sets.push_back(pke);
#endif
        }
        else
        {
          /* This is impossible to happen in case of primary keys */
          DBUG_ASSERT(key_number !=0);
        }
      }
    }

    /*
      Foreign keys handling.
      We check the foreign keys existence here and not at check_foreign_key()
      function to avoid allocate foreign_key_map when it is not needed.

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
    if (!(thd->variables.option_bits & OPTION_NO_FOREIGN_KEY_CHECKS))
    {
      std::map<std::string,std::string> foreign_key_map;
      check_foreign_key(table, thd, foreign_key_map);

      if (!foreign_key_map.empty())
      {
        for (uint i=0; i < table->s->fields; i++)
        {
          /* Ignore if the value is NULL. */
          if (table->field[i]->is_null())
            continue;

          std::map<std::string,std::string>::iterator it=
              foreign_key_map.find(table->s->field[i]->field_name);
          if (foreign_key_map.end() != it)
          {
            for (int collation_conversion_algorithm= COLLATION_CONVERSION_ALGORITHM;
                 collation_conversion_algorithm >= 0;
                 collation_conversion_algorithm--)
            {
              std::string pke_prefix= it->second;
              size_t length= 0;

              // convert using collation support conversion algorithm
              if (COLLATION_CONVERSION_ALGORITHM == collation_conversion_algorithm)
              {
                const CHARSET_INFO* cs= table->field[i]->charset();
                length= cs->coll->strnxfrmlen(cs,
                                         table->field[i]->pack_length());
              }
              // convert using without collation support algorithm
              else
              {
                table->field[i]->val_str(&row_data);
                length= row_data.length();
              }

              if (pk_value_size < length+1)
              {
                pk_value_size= length+1;
                pk_value= (char*) my_realloc(key_memory_write_set_extraction,
                                             pk_value, pk_value_size,
                                             MYF(MY_ZEROFILL));
              }

              // convert using collation support conversion algorithm
              if (COLLATION_CONVERSION_ALGORITHM == collation_conversion_algorithm)
              {
                /*
                  convert to normalized string and store so that it can be
                  sorted using binary comparison functions like memcmp.
                */
                table->field[i]->make_sort_key((uchar*)pk_value, length);
                pk_value[length]= 0;
              }
              // convert using without collation support algorithm
              else
              {
                strmake(pk_value, row_data.c_ptr_safe(), length);
              }

              pke_prefix.append(pk_value, length);
              pke_prefix.append(HASH_STRING_SEPARATOR);
              value_length= my_safe_itoa(10, length,
                                         &value_length_buffer[VALUE_LENGTH_BUFFER_SIZE-1]);
              pke_prefix.append(value_length);

              generate_hash_pke(pke_prefix, collation_conversion_algorithm, thd);
              writeset_hashes_added++;
#ifndef DBUG_OFF
              write_sets.push_back(pke_prefix);
#endif
            }
          }
        }
      }
    }

    if (table->file->referenced_by_foreign_key())
      ws_ctx->set_has_related_foreign_keys();

    my_free(pk_value);

#ifndef DBUG_OFF
    debug_check_for_write_sets(write_sets);
#endif
  }

  if (writeset_hashes_added == 0)
    ws_ctx->set_has_missing_keys();

  DBUG_VOID_RETURN;
}
