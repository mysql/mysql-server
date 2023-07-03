/*****************************************************************************

Copyright (c) 2007, 2023, Oracle and/or its affiliates.

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

/** @file handler/i_s.h
 InnoDB INFORMATION SCHEMA tables interface to MySQL.

 Created July 18, 2007 Vasil Dimov
 *******************************************************/

#ifndef i_s_h
#define i_s_h

class THD;
class Table_ref;

extern struct st_mysql_plugin i_s_innodb_trx;
extern struct st_mysql_plugin i_s_innodb_cmp;
extern struct st_mysql_plugin i_s_innodb_cmp_reset;
extern struct st_mysql_plugin i_s_innodb_cmp_per_index;
extern struct st_mysql_plugin i_s_innodb_cmp_per_index_reset;
extern struct st_mysql_plugin i_s_innodb_cmpmem;
extern struct st_mysql_plugin i_s_innodb_cmpmem_reset;
extern struct st_mysql_plugin i_s_innodb_metrics;
extern struct st_mysql_plugin i_s_innodb_ft_default_stopword;
extern struct st_mysql_plugin i_s_innodb_ft_deleted;
extern struct st_mysql_plugin i_s_innodb_ft_being_deleted;
extern struct st_mysql_plugin i_s_innodb_ft_index_cache;
extern struct st_mysql_plugin i_s_innodb_ft_index_table;
extern struct st_mysql_plugin i_s_innodb_ft_config;
extern struct st_mysql_plugin i_s_innodb_buffer_page;
extern struct st_mysql_plugin i_s_innodb_buffer_page_lru;
extern struct st_mysql_plugin i_s_innodb_buffer_stats;
extern struct st_mysql_plugin i_s_innodb_temp_table_info;
extern struct st_mysql_plugin i_s_innodb_tables;
extern struct st_mysql_plugin i_s_innodb_tablestats;
extern struct st_mysql_plugin i_s_innodb_indexes;
extern struct st_mysql_plugin i_s_innodb_columns;
extern struct st_mysql_plugin i_s_innodb_foreign;
extern struct st_mysql_plugin i_s_innodb_foreign_cols;
extern struct st_mysql_plugin i_s_innodb_tablespaces;
extern struct st_mysql_plugin i_s_innodb_datafiles;
extern struct st_mysql_plugin i_s_innodb_virtual;
extern struct st_mysql_plugin i_s_innodb_cached_indexes;
extern struct st_mysql_plugin i_s_innodb_session_temp_tablespaces;

#endif /* i_s_h */
