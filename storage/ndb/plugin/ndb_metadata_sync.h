/*
   Copyright (c) 2019, Oracle and/or its affiliates. All rights reserved.

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

#ifndef NDB_METADATA_SYNC_H
#define NDB_METADATA_SYNC_H

#include <list>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#include "storage/ndb/include/ndbapi/NdbDictionary.hpp"  // NdbDictionary::Table

// Forward declarations
class Ndb;
class THD;
struct SHOW_VAR;
namespace dd {
class Table;
}

enum object_detected_type {
  LOGFILE_GROUP_OBJECT = 0,
  TABLESPACE_OBJECT,
  SCHEMA_OBJECT,
  TABLE_OBJECT
};

enum object_validation_state { PENDING = 0, IN_PROGRESS, DONE };

class Ndb_metadata_sync {
  struct Detected_object {
    std::string
        m_schema_name;   // Schema name, "" for logfile groups & tablespaces
    std::string m_name;  // Object name, "" for schema objects
    object_detected_type m_type;
    object_validation_state m_validation_state;  // Used for blacklist only

    Detected_object(std::string schema_name, std::string name,
                    object_detected_type type)
        : m_schema_name{std::move(schema_name)},
          m_name{std::move(name)},
          m_type{type},
          m_validation_state{object_validation_state::PENDING} {}
  };

  mutable std::mutex m_objects_mutex;  // protects m_objects
  std::list<Detected_object> m_objects;
  mutable std::mutex m_blacklist_mutex;  // protects m_blacklist
  std::vector<Detected_object> m_blacklist;

  /*
    @brief Construct a string comprising of the object type and name. This is
           used in log messages

    @param object  Detected object

    @return string comprising of object type and name
  */
  std::string object_type_and_name_str(const Detected_object &object) const;

  /*
    @brief Check if an object has been detected already and is currently
           waiting in the queue of objects to be synchronized

    @param object  Details of object to be checked

    @return true if object is already in the queue, false if not
  */
  bool object_sync_pending(const Detected_object &object) const;

  /*
    @brief Check if an object is present in the current blacklist of objects.
           Objects present in the blacklist will be ignored

    @param object  Details of object to be checked

    @return true if object is in the blacklist, false if not
  */
  bool object_blacklisted(const Detected_object &object) const;

  /*
    @brief Drop NDB_SHARE

    @param schema_name  Name of the schema
    @param table_name   Name of the table

    @return void
  */
  void drop_ndb_share(const char *schema_name, const char *table_name) const;

  /*
    @brief Get details of an object pending validation from the current
           blacklist of objects

    @param schema_name [out]  Name of the schema
    @param name        [out]  Name of the object
    @param type        [out]  Type of the object

    @return true if an object pending validation was found, false if not
  */
  bool get_blacklist_object_for_validation(std::string &schema_name,
                                           std::string &name,
                                           object_detected_type &type);

  /*
    @brief Check if a mismatch still exists for the retrieved blacklist object

    @param thd      Thread handle
    @param schema_name  Name of the schema
    @param name         Name of the object
    @param type         Type of the object

    @return true if mismatch still exists, false if not
  */
  bool check_blacklist_object_mismatch(THD *thd, const std::string &schema_name,
                                       const std::string &name,
                                       object_detected_type type) const;

  /*
    @brief Validate object in the blacklist. The object being validated is
           either removed from the blacklist if the mismatch doesn't exist any
           more or kept in the blacklist and marked as validated for this
           validation cycle

    @param check_mismatch_result  Denotes whether the mismatch exists or not

    @return void
  */
  void validate_blacklist_object(bool check_mismatch_result);

  /*
    @brief Reset the state of all blacklist objects to pending validation at the
           end of a validation cycle

    @return void
  */
  void reset_blacklist_state();

 public:
  Ndb_metadata_sync() {}
  Ndb_metadata_sync(const Ndb_metadata_sync &) = delete;

  /*
    @brief Add a logfile group to the back of the queue of objects to be
           synchronized

    @param logfile_group_name  Name of the logfile group

    @return true if logfile group successfully added, false if not
  */
  bool add_logfile_group(const std::string &logfile_group_name);

  /*
    @brief Add a tablespace to the back of the queue of objects to be
           synchronized

    @param tablespace_name  Name of the tablespace

    @return true if tablespace successfully added, false if not
  */
  bool add_tablespace(const std::string &tablespace_name);

  /*
    @brief Add a schema to the back of the queue of objects to be synchronized

    @param schema_name  Name of the schema

    @return true if the schema is successfully added, false if not
  */
  bool add_schema(const std::string &schema_name);

  /*
    @brief Add a table to the back of the queue of objects to be synchronized

    @param schema_name  Name of the schema
    @param table_name   Name of the table

    @return true if table successfully added, false if not
  */
  bool add_table(const std::string &schema_name, const std::string &table_name);

  /*
    @brief Check if the queue of objects to be synchronized is currently empty

    @return true if the queue is empty, false if not
  */
  bool object_queue_empty() const;

  /*
    @brief Retrieve details of the object currently the front of the queue. Note
           that this object is also removed from the queue

    @param schema_name [out]  Name of the schema
    @param name        [out]  Name of the object
    @param type        [out]  Type of the object
    @return void
  */
  void get_next_object(std::string &schema_name, std::string &name,
                       object_detected_type &type);

  /*
    @brief Add an object to the blacklist

    @param schema_name  Name of the schema
    @param name         Name of the object
    @param type         Type of the object

    @return void
  */
  void add_object_to_blacklist(const std::string &schema_name,
                               const std::string &name,
                               object_detected_type type);

  /*
    @brief Iterate through the blacklist of objects and check if the mismatches
           are still present or if the user has manually synchronized the
           objects

    @param thd  Thread handle

    @return void
  */
  void validate_blacklist(THD *thd);

  /*
    @brief Synchronize a logfile group object between NDB Dictionary and DD

    @param thd                 Thread handle
    @param logfile_group_name  Name of the logfile group
    @param temp_error [out]    Denotes if the failure was due to a temporary
                               error

    @return true if the logfile group was synced successfully, false if not
  */
  bool sync_logfile_group(THD *thd, const std::string &logfile_group_name,
                          bool &temp_error) const;

  /*
    @brief Synchronize a tablespace object between NDB Dictionary and DD

    @param thd               Thread handle
    @param tablespace_name   Name of the tablespace
    @param temp_error [out]  Denotes if the failure was due to a temporary error

    @return true if the tablespace was synced successfully, false if not
  */
  bool sync_tablespace(THD *thd, const std::string &tablespace_name,
                       bool &temp_error) const;

  /*
    @brief Synchronize a schema object between NDB Dictionary and DD

    @param thd               Thread handle
    @param schema_name       Name of the schema
    @param temp_error [out]  Denotes if the failure was due to a temporary error

    @return true if the schema was synced successfully, false if not
  */
  bool sync_schema(THD *thd, const std::string &schema_name,
                   bool &temp_error) const;

  /*
    @brief Synchronize a table object between NDB Dictionary and DD

    @param thd               Thread handle
    @param schema_name       Name of the schema the table belongs to
    @param table_name        Name of the table
    @param temp_error [out]  Denotes if the failure was due to a temporary error

    @return true if the table was synced successfully, false if not
  */
  bool sync_table(THD *thd, const std::string &schema_name,
                  const std::string &table_name, bool &temp_error);
};

/*
  Called as part of SHOW STATUS or performance_schema queries. Returns
  information about the number of NDB metadata objects currently in the
  blacklist
*/
int show_ndb_metadata_blacklist_size(THD *, SHOW_VAR *var, char *);

#endif
