/*****************************************************************************

Copyright (c) 2000, 2019, Alibaba and/or its affiliates. All Rights Reserved.

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

/** @file include/srv0file.h
 Interface of data file purge operation.

 Created 1/11/2019 Jianwei Zhao
 *******************************************************/
#ifndef srv0file_h
#define srv0file_h


#ifdef UNIV_PFS_THREAD
/* File purge thread PFS key */
extern mysql_pfs_key_t srv_file_purge_thread_key;
#endif

#ifdef UNIV_PFS_MUTEX
/* File purge list mutex PFS key */
extern mysql_pfs_key_t file_purge_list_mutex_key;
#endif

/** Whether enable the data file purge asynchronously little by little */
extern bool srv_data_file_purge;

/** Whether unlink the file immediately by purge thread */
extern bool srv_data_file_purge_immediate;

/** Whether purge all when normal shutdown */
extern bool srv_data_file_purge_all_at_shutdown;

/** Time interval (milliseconds) every data file purge operation */
extern ulong srv_data_file_purge_interval;

/** Max size (MB) every data file purge operation */
extern ulong srv_data_file_purge_max_size;

/** The directory that purged data file will be removed into */
extern char *srv_data_file_purge_dir;

/** Whether to print data file purge process */
extern bool srv_print_data_file_purge_process;

/** Data file purge system initialize when InnoDB server boots */
extern void srv_file_purge_init();

/** Data file purge system destroy when InnoDB server shutdown */
extern void srv_file_purge_destroy();

/** Data file purge thread runtime */
extern void srv_file_purge_thread(void);

/** Wakeup the background thread when shutdown */
extern void srv_wakeup_file_purge_thread(void);

#endif
