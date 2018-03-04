/*
   Copyright (c) 2000, 2017, Oracle and/or its affiliates. All rights reserved.

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

#include <stddef.h>

namespace dd {
class Table;
}

/*
  Initialize the binlog part of the ndb handlerton
*/
void ndbcluster_binlog_init(struct handlerton* hton);

int ndbcluster_binlog_setup_table(class THD* thd, class Ndb* ndb,
                                  const char* db, const char* table_name,
                                  const dd::Table* table_def);

int ndbcluster_binlog_wait_synch_drop_table(class THD* thd,
                                            struct NDB_SHARE* share);

int ndbcluster_binlog_start();

void ndbcluster_binlog_set_server_started();

int ndbcluster_binlog_end();

/*
  Will return true when the ndb binlog component is properly setup
  and ready to receive events from the cluster. As long as function
  returns false, all tables in this MySQL Server are opened in read only
  mode to avoid writes before the binlog is ready to record them.
 */
bool ndb_binlog_is_read_only(void);

/* Prints ndb binlog status string in buf */
size_t ndbcluster_show_status_binlog(char* buf, size_t buf_size);
