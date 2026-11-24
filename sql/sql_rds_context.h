/*****************************************************************************

Copyright (c) 2025, Alibaba and/or its affiliates. All Rights Reserved.

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

#ifndef SQL_RDS_CONTEXT_H
#define SQL_RDS_CONTEXT_H

/**
   RDS-specific data structures that need to be stored in the class THD.

   Init it when THD is created, and destroy when THD is destroyed.
 */

class THD_rds_context {
 public:
  THD_rds_context()
      : copy_ddl_from_innodb_to_duckdb(false),
        duckdb_parallel_copy_ddl(false),
        copy_ddl_from_duckdb_to_duckdb(false) {}

  void reset_each_stmt() {
    copy_ddl_from_innodb_to_duckdb = false;
    duckdb_parallel_copy_ddl = false;
    copy_ddl_from_duckdb_to_duckdb = false;
    copy_ddl_from_duckdb_to_duckdb = false;
  }
  bool is_copy_ddl_from_innodb_to_duckdb() {
    return copy_ddl_from_innodb_to_duckdb;
  }

  void set_copy_ddl_from_innodb_to_duckdb(bool value) {
    copy_ddl_from_innodb_to_duckdb = value;
  }

  bool is_duckdb_parallel_copy_ddl() { return duckdb_parallel_copy_ddl; }
  void set_duckdb_parallel_copy_ddl(bool value) {
    duckdb_parallel_copy_ddl = value;
  }

  bool is_copy_ddl_from_duckdb_to_duckdb() {
    return copy_ddl_from_duckdb_to_duckdb;
  }
  void set_copy_ddl_from_duckdb_to_duckdb(bool value) {
    copy_ddl_from_duckdb_to_duckdb = value;
  }

 private:
  /* Current connection is executing COPY DDL from InnoDB to DuckDB. */
  bool copy_ddl_from_innodb_to_duckdb;

  /* The state of duckdb parallel copy ddl. */
  bool duckdb_parallel_copy_ddl;

  /* Current connection is executing COPY DDL from DuckDB to DuckDB. */
  bool copy_ddl_from_duckdb_to_duckdb;
};

#endif  // SQL_RDS_CONTEXT_H
