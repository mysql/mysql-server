/*****************************************************************************

Copyright (c) 2007, 2009, Innobase Oy. All Rights Reserved.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc., 59 Temple
Place, Suite 330, Boston, MA 02111-1307 USA

*****************************************************************************/

/**************************************************//**
@file handler/i_s.h
InnoDB INFORMATION SCHEMA tables interface to MySQL.

Created July 18, 2007 Vasil Dimov
*******************************************************/

#ifndef i_s_h
#define i_s_h

extern struct st_mysql_plugin	i_s_innodb_buffer_pool_pages;
extern struct st_mysql_plugin	i_s_innodb_buffer_pool_pages_index;
extern struct st_mysql_plugin	i_s_innodb_buffer_pool_pages_blob;
extern struct st_mysql_plugin	i_s_innodb_trx;
extern struct st_mysql_plugin	i_s_innodb_locks;
extern struct st_mysql_plugin	i_s_innodb_lock_waits;
extern struct st_mysql_plugin	i_s_innodb_cmp;
extern struct st_mysql_plugin	i_s_innodb_cmp_reset;
extern struct st_mysql_plugin	i_s_innodb_cmpmem;
extern struct st_mysql_plugin	i_s_innodb_cmpmem_reset;
extern struct st_mysql_plugin	i_s_innodb_rseg;
extern struct st_mysql_plugin	i_s_innodb_table_stats;
extern struct st_mysql_plugin	i_s_innodb_index_stats;
extern struct st_mysql_plugin	i_s_innodb_admin_command;
extern struct st_mysql_plugin   i_s_innodb_sys_tables;
extern struct st_mysql_plugin   i_s_innodb_sys_indexes;
extern struct st_mysql_plugin	i_s_innodb_sys_stats;
extern struct st_mysql_plugin	i_s_innodb_changed_pages;
extern struct st_mysql_plugin	i_s_innodb_buffer_page;
extern struct st_mysql_plugin	i_s_innodb_buffer_page_lru;
extern struct st_mysql_plugin	i_s_innodb_buffer_stats;

extern struct st_maria_plugin i_s_innodb_buffer_pool_pages_maria;
extern struct st_maria_plugin i_s_innodb_buffer_pool_pages_index_maria;
extern struct st_maria_plugin i_s_innodb_buffer_pool_pages_blob_maria;
extern struct st_maria_plugin i_s_innodb_trx_maria;
extern struct st_maria_plugin i_s_innodb_locks_maria;
extern struct st_maria_plugin i_s_innodb_lock_waits_maria;
extern struct st_maria_plugin i_s_innodb_cmp_maria;
extern struct st_maria_plugin i_s_innodb_cmp_reset_maria;
extern struct st_maria_plugin i_s_innodb_cmpmem_maria;
extern struct st_maria_plugin i_s_innodb_cmpmem_reset_maria;
extern struct st_maria_plugin i_s_innodb_rseg_maria;
extern struct st_maria_plugin i_s_innodb_table_stats_maria;
extern struct st_maria_plugin i_s_innodb_index_stats_maria;
extern struct st_maria_plugin i_s_innodb_admin_command_maria;
extern struct st_maria_plugin i_s_innodb_sys_tables_maria;
extern struct st_maria_plugin i_s_innodb_sys_indexes_maria;
extern struct st_maria_plugin i_s_innodb_sys_stats_maria;
extern struct st_maria_plugin i_s_innodb_changed_pages_maria;
extern struct st_maria_plugin i_s_innodb_buffer_page_maria;
extern struct st_maria_plugin i_s_innodb_buffer_page_lru_maria;
extern struct st_maria_plugin i_s_innodb_buffer_stats_maria;

#endif /* i_s_h */
