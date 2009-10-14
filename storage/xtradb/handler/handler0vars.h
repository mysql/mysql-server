/*****************************************************************************

Copyright (c) 2008, 2009, Innobase Oy. All Rights Reserved.

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

/***********************************************************************
This file contains accessor functions for dynamic plugin on Windows.
***********************************************************************/

#if defined __WIN__ && defined MYSQL_DYNAMIC_PLUGIN
/***********************************************************************
This is a list of externals that can not be resolved by delay loading.
They have to be resolved indirectly via their addresses in the .map file.
All of them are external variables. */
extern	MYSQL_PLUGIN_IMPORT CHARSET_INFO		my_charset_bin;
extern	MYSQL_PLUGIN_IMPORT CHARSET_INFO		my_charset_latin1;
extern	MYSQL_PLUGIN_IMPORT CHARSET_INFO		my_charset_filename;
extern	MYSQL_PLUGIN_IMPORT CHARSET_INFO*		system_charset_info;
extern	MYSQL_PLUGIN_IMPORT CHARSET_INFO*		default_charset_info;
//extern	MYSQL_PLUGIN_IMPORT CHARSET_INFO**		all_charsets;
extern	MYSQL_PLUGIN_IMPORT system_variables	global_system_variables;
//extern	MYSQL_PLUGIN_IMPORT char*			mysql_real_data_home;
extern	MYSQL_PLUGIN_IMPORT char*			mysql_data_home;
//extern	MYSQL_PLUGIN_IMPORT char**			tx_isolation_names;
//extern	MYSQL_PLUGIN_IMPORT char**			binlog_format_names;
//extern	MYSQL_PLUGIN_IMPORT char			reg_ext;
extern	MYSQL_PLUGIN_IMPORT pthread_mutex_t	LOCK_thread_count;
extern	MYSQL_PLUGIN_IMPORT key_map		key_map_full;
extern	MYSQL_PLUGIN_IMPORT MY_TMPDIR		mysql_tmpdir_list;
extern	MYSQL_PLUGIN_IMPORT bool			mysqld_embedded;
extern	MYSQL_PLUGIN_IMPORT uint			lower_case_table_names;
extern	MYSQL_PLUGIN_IMPORT ulong			specialflag;
extern	MYSQL_PLUGIN_IMPORT int			my_umask;

extern  MYSQL_PLUGIN_IMPORT char *relay_log_info_file;

/*
#define my_charset_bin		(*wdl_my_charset_bin)
#define my_charset_latin1	(*wdl_my_charset_latin1)
#define my_charset_filename	(*wdl_my_charset_filename)
#define system_charset_info	(*wdl_system_charset_info)
#define default_charset_info	(*wdl_default_charset_info)
#define all_charsets		(wdl_all_charsets)
#define global_system_variables	(*wdl_global_system_variables)
#define mysql_real_data_home	(wdl_mysql_real_data_home)
#define mysql_data_home		(*wdl_mysql_data_home)
#define tx_isolation_names	(wdl_tx_isolation_names)
#define binlog_format_names	(wdl_binlog_format_names)
#define reg_ext			(wdl_reg_ext)
#define LOCK_thread_count	(*wdl_LOCK_thread_count)
#define key_map_full		(*wdl_key_map_full)
#define mysql_tmpdir_list	(*wdl_mysql_tmpdir_list)
#define mysqld_embedded		(*wdl_mysqld_embedded)
*/
//#define lower_case_table_names	(*wdl_lower_case_table_names)
//#define specialflag		(*wdl_specialflag)
//#define my_umask		(*wdl_my_umask)

#endif
