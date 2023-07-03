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

#ifndef NDB_THD_H
#define NDB_THD_H

#include "mysql/plugin.h"

extern handlerton *ndbcluster_hton;

/* Get Thd_ndb pointer from THD */
static inline class Thd_ndb *get_thd_ndb(const THD *thd) {
  return (class Thd_ndb *)thd_get_ha_data(thd, ndbcluster_hton);
}

/* Set Thd_ndb pointer for THD */
static inline void thd_set_thd_ndb(THD *thd, class Thd_ndb *thd_ndb) {
  thd_set_ha_data(thd, ndbcluster_hton, thd_ndb);
}

/* Make sure THD has a Thd_ndb struct assigned */
class Ndb *check_ndb_in_thd(THD *thd, bool validate_ndb = false);

/*
  Determine if THD is applying binlog. ie. either marked as
  slave thread or being in "pseudo slave mode"
*/
bool applying_binlog(const THD *thd);

/*
  Return the THD's unmasked server id
*/
uint32 thd_unmasked_server_id(const THD *thd);

/*
 @brief Return the THD's current query string

 @note It's safe for own thread to read it's query string
*/
const char *ndb_thd_query(const THD *thd);

/*
 @brief Return the length of THD's current query

 @note It's safe for own thread to read it's query string length
*/
size_t ndb_thd_query_length(const THD *thd);

/*
 @brief Check if THD is the "binlog injector thread"

 @return true if thread matches condition
*/
bool ndb_thd_is_binlog_thread(const THD *thd);

/*
 @brief Check if THD is a "background thread"

 @return true if thread matches condition
*/
bool ndb_thd_is_background_thread(const THD *thd);

/*
 @brief Register ndbcluster for a statement and optionally a transaction.

 @param thd               Thread object
 @param register_trans    Boolean flag to control registering for a transaction.
                          Ndbcluster is registered for a transaction only if
                          this is true.

 @note trans_register_ha() is idempotent. So this function is idempotent too.
*/
void ndb_thd_register_trans(THD *thd, bool register_trans);

/*
  @brief Clear conditions accumulated in THD

  @param thd  Thread handle
*/
void clear_thd_conditions(THD *thd);

enum condition_logging_level { INFO, WARNING, ERROR };
/*
  @brief Log and then clear conditions accumulated in THD

  @param thd            Thread handle
  @param logging_level  Level of the log messages i.e. info, warning, or error
*/
void log_and_clear_thd_conditions(THD *thd,
                                  condition_logging_level logging_level);

#endif
