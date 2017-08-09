/* Copyright (c) 2000, 2017, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */

#ifndef RPL_FILTER_H
#define RPL_FILTER_H

#include "my_config.h"

#include <stddef.h>
#include <sys/types.h>
#include <atomic>
#include <memory>
#include <string>
#include <vector>

#include "map_helpers.h"
#include "my_inttypes.h"
#include "my_sqlcommand.h"
#include "prealloced_array.h"                   // Prealloced_arrray
#include "sql/options_mysqld.h"                 // options_mysqld
#include "sql/rpl_gtid.h"
#include "sql/sql_cmd.h"                        // Sql_cmd
#include "sql/sql_list.h"                       // I_List
#include "sql_string.h"

class Item;
class String;
class THD;
struct TABLE_LIST;


typedef struct st_table_rule_ent
{
  char* db;
  char* tbl_name;
  uint key_len;
} TABLE_RULE_ENT;


/** Enum values for CONFIGURED_BY column. */
enum enum_configured_by
{
  CONFIGURED_BY_STARTUP_OPTIONS= 1,
  CONFIGURED_BY_CHANGE_REPLICATION_FILTER,
  CONFIGURED_BY_STARTUP_OPTIONS_FOR_CHANNEL,
  CONFIGURED_BY_CHANGE_REPLICATION_FILTER_FOR_CHANNEL
};


/*
  Rpl_filter_statistics

  The statistics for replication filter
*/
class Rpl_filter_statistics
{
public:
  Rpl_filter_statistics();
  ~Rpl_filter_statistics();
  void set_all(enum_configured_by configured_by, ulonglong counter);

  void set_all(enum_configured_by configured_by, ulonglong counter,
               ulonglong active_since)
  {
    m_configured_by= configured_by;
    m_atomic_counter= counter;
    m_active_since= active_since;
  }

  void reset()
  {
    set_all(CONFIGURED_BY_STARTUP_OPTIONS, 0, 0);
  }

  enum_configured_by get_configured_by()
  {
    return m_configured_by;
  }
  ulonglong get_active_since()
  {
    return m_active_since;
  }
  ulonglong get_counter()
  {
    return m_atomic_counter;
  }
  void increase_counter()
  {
    m_atomic_counter++;
  }

private:
  /*
    The replication filters can be configured with the following four states:
    STARTUP_OPTIONS, //STARTUP_OPTIONS: --REPLICATE-*
    CHANGE_REPLICATION_FILTER, //CHANGE REPLICATION FILTER filter [, filter...]
    STARTUP_OPTIONS_FOR_CHANNEL, //STARTUP_OPTIONS: --REPLICATE-* (FOR_CHANNEL)
    CHANGE_REPLICATION_FILTER_FOR_CHANNEL //CHANGE REPLICATION FILTER filter [,
                                          filter...] FOR CHANNEL <channel_name>
  */
  enum_configured_by m_configured_by;

  /* Timestamp of when the configuration took place */
  ulonglong m_active_since;

  /*
    The hit counter of the filter since last configuration.
    The m_atomic_counter may be increased by concurrent slave
    workers, so we use the atomic<uint64>.
  */
  std::atomic<uint64> m_atomic_counter{0};

  /* Prevent user from invoking default constructor function. */
  Rpl_filter_statistics(Rpl_filter_statistics const&);

  /* Prevent user from invoking default assignment function. */
  Rpl_filter_statistics& operator=(Rpl_filter_statistics const&);
};


/*
  Rpl_pfs_filter

  The helper class for filling the replication
  performance_schema.replication_applier_filters table and
  performance_schema.replication_applier_global_filters table.
*/
class Rpl_pfs_filter
{
public:
  Rpl_pfs_filter();
  ~Rpl_pfs_filter();

  void set_channel_name(const char* channel_name)
  {
    m_channel_name= channel_name;
  }
  void set_filter_name(const char* filter_name)
  {
    m_filter_name= filter_name;
  }
  void set_filter_rule(const String& filter_rule)
  {
    m_filter_rule.copy(filter_rule);
  }

  const char* get_channel_name()
  {
    return m_channel_name;
  }
  const char* get_filter_name()
  {
    return m_filter_name;
  }
  const String& get_filter_rule()
  {
    return m_filter_rule;
  }

  /* An object to replication filter statistics. */
  Rpl_filter_statistics m_rpl_filter_statistics;

private:

  /* A pointer to the channel name. */
  const char* m_channel_name;

  /* A pointer to the filer name. */
  const char* m_filter_name;

  /* A filter rule. */
  String m_filter_rule;

  /* Prevent user from invoking default constructor function. */
  Rpl_pfs_filter(Rpl_pfs_filter const&);

  /* Prevent user from invoking default assignment function. */
  Rpl_pfs_filter& operator=(Rpl_pfs_filter const&);
};


/*
  Rpl_filter

  Inclusion and exclusion rules of tables and databases.
  Also handles rewrites of db.
  Used for replication and binlogging.
 */
class Rpl_filter 
{
public:
  Rpl_filter();
  ~Rpl_filter();
  Rpl_filter(Rpl_filter const&);
  Rpl_filter& operator=(Rpl_filter const&);
 
  /* Checks - returns true if ok to replicate/log */

  bool tables_ok(const char* db, TABLE_LIST* tables);
  bool db_ok(const char* db, bool need_increase_counter= true);
  bool db_ok_with_wild_table(const char *db);

  bool is_on();
  /**
    Check if the replication filter is empty or not.

    @retval true if the replication filter is empty.
    @retval false if the replication filter is not empty.
  */
  bool is_empty();
  /**
    Copy global replication filters to its per-channel replication filters
    if there are no per-channel replication filters and there are global
    filters on the filter type on channel creation.

    @retval 0 OK
    @retval 1 Error
  */
  int copy_global_replication_filters();

  bool is_rewrite_empty();

  /* Setters - add filtering rules */
  int build_do_table_hash();
  int build_ignore_table_hash();

  int add_string_list(I_List<i_string> *list, const char* spec);
  int add_string_pair_list(I_List<i_string_pair> *list, char* key, char *val);
  int add_do_table_array(const char* table_spec);
  int add_ignore_table_array(const char* table_spec);

  int add_wild_do_table(const char* table_spec);
  int add_wild_ignore_table(const char* table_spec);

  int set_do_db(List<Item> *list, enum_configured_by configured_by);
  int set_ignore_db(List<Item> *list, enum_configured_by configured_by);
  int set_do_table(List<Item> *list, enum_configured_by configured_by);
  int set_ignore_table(List<Item> *list, enum_configured_by configured_by);
  int set_wild_do_table(List<Item> *list, enum_configured_by configured_by);
  int set_wild_ignore_table(List<Item> *list,
                            enum_configured_by configured_by);
  int set_db_rewrite(List<Item> *list, enum_configured_by configured_by);
  typedef int (Rpl_filter::*Add_filter)(char const*);
  int parse_filter_list(List<Item> *item_list, Add_filter func);
  /**
    Execute the specified func with elements of the list as input.

    @param list A list with I_List<i_string> type
    @param add A function with Add_filter type

    @retval 0 OK
    @retval 1 Error
  */
  int parse_filter_list(I_List<i_string> *list, Add_filter add);
  int add_do_db(const char* db_spec);
  int add_ignore_db(const char* db_spec);

  int add_db_rewrite(const char* from_db, const char* to_db);

  /* Getters - to get information about current rules */

  void get_do_table(String* str);
  void get_ignore_table(String* str);

  void get_wild_do_table(String* str);
  void get_wild_ignore_table(String* str);

  const char* get_rewrite_db(const char* db, size_t *new_len);
  void get_rewrite_db(String *str);

  I_List<i_string>* get_do_db();
  /*
    Get do_db rule.

    @param[out] str the db_db rule.
  */
  void get_do_db(String *str);

  I_List<i_string>* get_ignore_db();
  /*
    Get ignore_db rule.

    @param[out] str the ignore_db rule.
  */
  void get_ignore_db(String *str);
  /*
    Get rewrite_db_statistics.

    @retval A pointer to a rewrite_db_statistics object.
  */
  Rpl_filter_statistics* get_rewrite_db_statistics()
  {
    return &rewrite_db_statistics;
  }

  void free_string_list(I_List<i_string> *l);
  void free_string_pair_list(I_List<i_string_pair> *l);

#ifdef WITH_PERFSCHEMA_STORAGE_ENGINE
  /**
    Used only by replication performance schema indices to get the count
    of replication filters.

    @retval the count of global replication filters.
  */
  uint get_filter_count();
  /**
    Put replication filters with attached channel name into a vector.

    @param rpl_pfs_filter_vec the vector.
    @param channel_name the name of the channel attached or NULL if
                        there is no channel attached.
  */
  void put_filters_into_vector(
    std::vector<Rpl_pfs_filter*>& rpl_pfs_filter_vec,
    const char* channel_name);
  /**
    Used only by replication performance schema indices to get the global
    replication filter at the position 'pos' from the
    rpl_pfs_global_filter_vec vector.

    @param pos the index in the rpl_pfs_filter_vec vector.

    @retval Rpl_filter A pointer to a Rpl_pfs_filter, or NULL if it
                       arrived the end of the rpl_pfs_filter_vec.
  */
  Rpl_pfs_filter* get_global_filter_at_pos(uint pos);

  /**
    This member function shall reset the P_S view associated
    with the filters.
   */
  void reset_pfs_view();
#endif /* WITH_PERFSCHEMA_STORAGE_ENGINE */

  /**
    Delete all objects in the rpl_pfs_global_filter_vec vector
    and then clear the vector.
  */
  void cleanup_rpl_pfs_global_filter_vec()
  {
    /* Delete all objects in the rpl_pfs_global_filter_vec vector. */
    std::vector<Rpl_pfs_filter*>::iterator it;
    for(it= rpl_pfs_global_filter_vec.begin();
        it != rpl_pfs_global_filter_vec.end(); ++it)
    {
      delete(*it);
      *it= NULL;
    }

    rpl_pfs_global_filter_vec.clear();
  }

  /**
    Acquire the write lock.
  */
  void wrlock()
  { m_rpl_filter_lock->wrlock(); }

  /**
    Acquire the read lock.
  */
  void rdlock()
  { m_rpl_filter_lock->rdlock(); }

  /**
    Release the lock (whether it is a write or read lock).
  */
  void unlock()
  { m_rpl_filter_lock->unlock(); }

  /**
    Check if the relation between the per-channel filter and
    the channel's Relay_log_info is established.

    @retval true if the relation is established
    @retval false if the relation is not established
  */
  bool is_attached()
  {
    return attached;
  }

  /**
    Set attached to true when the relation between the per-channel filter
    and the channel's Relay_log_info is established.
  */
  void set_attached()
  {
    attached= true;
  }

  void reset();

  Rpl_filter_statistics do_table_statistics;
  Rpl_filter_statistics ignore_table_statistics;
  Rpl_filter_statistics wild_do_table_statistics;
  Rpl_filter_statistics wild_ignore_table_statistics;
  Rpl_filter_statistics do_db_statistics;
  Rpl_filter_statistics ignore_db_statistics;
  Rpl_filter_statistics rewrite_db_statistics;


private:
  bool table_rules_on;
  /*
    State if the relation between the per-channel filter
    and the channel's Relay_log_info is established.
  */
  bool attached;
  /*
    Store pointers of all Rpl_pfs_filter objects in
    global replication filter.
  */
  std::vector<Rpl_pfs_filter*> rpl_pfs_global_filter_vec;

  /*
    While slave is not running after server startup, the replication filter
    can be modified by CHANGE REPLICATION FILTER filter [, filter...]
    [FOR CHANNEL <channel_name>] and CHANGE MASTER TO ... FOR CHANNEL,
    and read by querying P_S.replication_applier_global_filters,
    querying P_S.replication_applier_filters, and SHOW SLAVE STATUS
    [FOR CHANNEL <channel_name>]. So the lock is introduced to protect
    some member functions called by above commands. See below.

    The read lock should be held when calling the following member functions:
      get_do_table(String* str);  // SHOW SLAVE STATUS
      get_ignore_table(String* str); // SHOW SLAVE STATUS
      get_wild_do_table(String* str); // SHOW SLAVE STATUS
      get_wild_ignore_table(String* str); // SHOW SLAVE STATUS
      get_rewrite_db(const char* db, size_t *new_len); // SHOW SLAVE STATUS
      get_rewrite_db(String *str); // SHOW SLAVE STATUS
      get_do_db(); // SHOW SLAVE STATUS
      get_do_db(String *str);  // SHOW SLAVE STATUS
      get_ignore_db();  // SHOW SLAVE STATUS
      get_ignore_db(String *str);  // SHOW SLAVE STATUS
      put_filters_into_vector(...);  // query P_S tables
      get_filter_count();  // query P_S tables

    The write lock should be held when calling the following member functions:
      set_do_db(List<Item> *list); // CHANGE REPLICATION FILTER
      set_ignore_db(List<Item> *list);  // CHANGE REPLICATION FILTER
      set_do_table(List<Item> *list);  // CHANGE REPLICATION FILTER
      set_ignore_table(List<Item> *list); // CHANGE REPLICATION FILTER
      set_wild_do_table(List<Item> *list); // CHANGE REPLICATION FILTER
      set_wild_ignore_table(List<Item> *list); // CHANGE REPLICATION FILTER
      set_db_rewrite(List<Item> *list); // CHANGE REPLICATION FILTER
      copy_global_replication_filters(); // CHANGE MASTER TO ... FOR CHANNEL

    Please acquire a wrlock when modifying the replication filter (CHANGE
    REPLICATION FILTER filter [, filter...] [FOR CHANNEL <channel_name>]
    and CHANGE MASTER TO ... FOR CHANNEL).
    Please acqurie a rdlock when reading the replication filter (
    SELECT * FROM performance_schema.replication_applier_global_filters,
    SELECT * FROM performance_schema.replication_applier_filters and
    SHOW SLAVE STATUS [FOR CHANNEL <channel_name>]).

    Other member functions do not need the protection of the lock and we can
    access thd->rli_slave->rpl_filter to filter log event without the
    protection of the lock while slave is running, since the replication
    filter is read/modified by a single thread during server startup and
    there is no command can change it while slave is running.
  */
  Checkable_rwlock *m_rpl_filter_lock;

  typedef Prealloced_array<TABLE_RULE_ENT*, 16> Table_rule_array;
  typedef collation_unordered_map<
    std::string, unique_ptr_my_free<TABLE_RULE_ENT>> Table_rule_hash;

  void init_table_rule_hash(Table_rule_hash** h, bool* h_inited);
  void init_table_rule_array(Table_rule_array*, bool* a_inited);

  int add_table_rule_to_array(Table_rule_array* a, const char* table_spec);
  int add_table_rule_to_hash(
     Table_rule_hash* h, const char* table_spec, uint len);

  void free_string_array(Table_rule_array *a);

  void table_rule_ent_hash_to_str(
    String* s,
    Table_rule_hash* h,
    bool inited);
  /**
    Builds a Table_rule_array from a hash of TABLE_RULE_ENT. Cannot be used for
    any other hash, as it assumes that the hash entries are TABLE_RULE_ENT.

    @param table_array Pointer to the Table_rule_array to fill
    @param h Pointer to the hash to read
    @param inited True if the hash is initialized

    @retval 0 OK
    @retval 1 Error
  */
  int table_rule_ent_hash_to_array(
    Table_rule_array* table_array,
    Table_rule_hash* h,
    bool inited);
  /**
    Builds a destination Table_rule_array from a source Table_rule_array
    of TABLE_RULE_ENT.

    @param dest_array Pointer to the destination Table_rule_array to fill
    @param source_array Pointer to the source Table_rule_array to read
    @param inited True if the source Table_rule_array is initialized

    @retval 0 OK
    @retval 1 Error
  */
  int table_rule_ent_array_to_array(Table_rule_array* dest_array,
                                    Table_rule_array* source_array,
                                    bool inited);
  void table_rule_ent_dynamic_array_to_str(String* s, Table_rule_array* a,
                                           bool inited);
  TABLE_RULE_ENT* find_wild(Table_rule_array *a, const char* key, size_t len);

  int build_table_hash_from_array(
    Table_rule_array *table_array,
    Table_rule_hash **table_hash,
    bool array_inited, bool *hash_inited);

  /*
    Those 6 structures below are uninitialized memory unless the
    corresponding *_inited variables are "true".
  */
  /* For quick search */
  Table_rule_hash *do_table_hash{nullptr};
  Table_rule_hash *ignore_table_hash{nullptr};

  Table_rule_array do_table_array;
  Table_rule_array ignore_table_array;

  Table_rule_array wild_do_table;
  Table_rule_array wild_ignore_table;

  bool do_table_hash_inited;
  bool ignore_table_hash_inited;
  bool do_table_array_inited;
  bool ignore_table_array_inited;
  bool wild_do_table_inited;
  bool wild_ignore_table_inited;

  I_List<i_string> do_db;
  I_List<i_string> ignore_db;

  I_List<i_string_pair> rewrite_db;
};


/** Sql_cmd_change_repl_filter represents the command CHANGE REPLICATION
 * FILTER.
 */
class Sql_cmd_change_repl_filter : public Sql_cmd
{
public:
  /** Constructor.  */
  Sql_cmd_change_repl_filter():
    do_db_list(NULL), ignore_db_list(NULL),
    do_table_list(NULL), ignore_table_list(NULL),
    wild_do_table_list(NULL), wild_ignore_table_list(NULL),
    rewrite_db_pair_list(NULL)
  {}

  ~Sql_cmd_change_repl_filter()
  {}

  virtual enum_sql_command sql_command_code() const
  {
    return SQLCOM_CHANGE_REPLICATION_FILTER;
  }
  bool execute(THD *thd);

  void set_filter_value(List<Item>* item_list, options_mysqld filter_type);
  bool change_rpl_filter(THD* thd);

private:

  List<Item> *do_db_list;
  List<Item> *ignore_db_list;
  List<Item> *do_table_list;
  List<Item> *ignore_table_list;
  List<Item> *wild_do_table_list;
  List<Item> *wild_ignore_table_list;
  List<Item> *rewrite_db_pair_list;

};

extern Rpl_filter *rpl_filter;
extern Rpl_filter *binlog_filter;

#endif // RPL_FILTER_H
