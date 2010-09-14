/* Copyright (C) 2010 Monty Program Ab

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

extern struct st_maria_plugin compat_aliases;
extern char mysql_real_data_home[FN_REFLEN];
extern TYPELIB maria_recover_typelib;
extern TYPELIB maria_stats_method_typelib;
extern TYPELIB maria_translog_purge_type_typelib;
extern TYPELIB maria_sync_log_dir_typelib;
extern TYPELIB maria_group_commit_typelib;
extern struct st_mysql_storage_engine maria_storage_engine;
extern my_bool use_maria_for_temp_tables;
extern struct st_mysql_sys_var* system_variables[];
extern st_mysql_show_var status_variables[];
void copy_variable_aliases();
