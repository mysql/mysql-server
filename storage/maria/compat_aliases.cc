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

/*
  compatibility aliases for system and static variables
*/
#include <my_global.h>
#include <maria.h>
#include <mysql/plugin.h>
#include "ma_loghandler.h"
#include "compat_aliases.h"

ulong block_size_alias;
static MYSQL_SYSVAR_ULONG(block_size, block_size_alias,
       PLUGIN_VAR_RQCMDARG | PLUGIN_VAR_READONLY,
       "Deprecated, use --aria-block-size instead", 0, 0,
       MARIA_KEY_BLOCK_LENGTH, MARIA_MIN_KEY_BLOCK_LENGTH,
       MARIA_MAX_KEY_BLOCK_LENGTH, MARIA_MIN_KEY_BLOCK_LENGTH);

ulong checkpoint_interval_alias;
static MYSQL_SYSVAR_ULONG(checkpoint_interval, checkpoint_interval_alias,
       PLUGIN_VAR_RQCMDARG,
       "Deprecated, use --aria-checkpoint-interval instead",
       NULL, NULL, 30, 0, UINT_MAX, 1);

ulong force_start_after_recovery_failures_alias;
static MYSQL_SYSVAR_ULONG(force_start_after_recovery_failures, force_start_after_recovery_failures_alias,
       PLUGIN_VAR_RQCMDARG | PLUGIN_VAR_READONLY,
       "Deprecated, use --aria-force-start-after-recovery-failures instead",
       NULL, NULL, 0, 0, UINT_MAX8, 1);

my_bool page_checksum_alias;
static MYSQL_SYSVAR_BOOL(page_checksum, page_checksum_alias, 0,
       "Deprecated, use --aria-page-checksum instead", 0, 0, 1);

char *log_dir_path_alias;
static MYSQL_SYSVAR_STR(log_dir_path, log_dir_path_alias,
       PLUGIN_VAR_NOSYSVAR | PLUGIN_VAR_RQCMDARG | PLUGIN_VAR_READONLY,
       "Deprecated, use --aria-log-dir-path instead",
       NULL, NULL, mysql_real_data_home);

ulong log_file_size_alias;
static MYSQL_SYSVAR_ULONG(log_file_size, log_file_size_alias,
       PLUGIN_VAR_RQCMDARG,
       "Deprecated, use --aria-log-file-size instead",
       NULL, NULL, TRANSLOG_FILE_SIZE,
       TRANSLOG_MIN_FILE_SIZE, 0xffffffffL, TRANSLOG_PAGE_SIZE);

ulong group_commit_alias;
static MYSQL_SYSVAR_ENUM(group_commit, group_commit_alias,
       PLUGIN_VAR_RQCMDARG,
       "Deprecated, use --aria-group-commit instead",
       NULL, NULL,
       TRANSLOG_GCOMMIT_NONE, &maria_group_commit_typelib);

ulong group_commit_interval_alias;
static MYSQL_SYSVAR_ULONG(group_commit_interval, group_commit_interval_alias,
       PLUGIN_VAR_RQCMDARG,
       "Deprecated, use --aria-group-commit-interval instead",
       NULL, NULL, 0, 0, UINT_MAX, 1);

ulong log_purge_type_alias;
static MYSQL_SYSVAR_ENUM(log_purge_type, log_purge_type_alias,
       PLUGIN_VAR_RQCMDARG,
       "Deprecated, use --aria-log-purge-type instead",
       NULL, NULL, TRANSLOG_PURGE_IMMIDIATE,
       &maria_translog_purge_type_typelib);

ulonglong max_sort_file_size_alias;
static MYSQL_SYSVAR_ULONGLONG(max_sort_file_size, max_sort_file_size_alias,
       PLUGIN_VAR_RQCMDARG,
       "Deprecated, use --aria-max-temp-length instead",
       0, 0, MAX_FILE_SIZE, 0, MAX_FILE_SIZE, 1024*1024);

ulong pagecache_age_threshold_alias;
static MYSQL_SYSVAR_ULONG(pagecache_age_threshold, pagecache_age_threshold_alias,
       PLUGIN_VAR_RQCMDARG,
       "Deprecated, use --aria-pagecache-age-threshold instead",
       0, 0, 300, 100, ~0L, 100);

ulonglong pagecache_buffer_size_alias;
static MYSQL_SYSVAR_ULONGLONG(pagecache_buffer_size, pagecache_buffer_size_alias,
       PLUGIN_VAR_RQCMDARG | PLUGIN_VAR_READONLY,
       "Deprecated, use --aria-pagecache-buffer-size instead",
       0, 0, KEY_CACHE_SIZE, MALLOC_OVERHEAD, ~0UL, IO_SIZE);

ulong pagecache_division_limit_alias;
static MYSQL_SYSVAR_ULONG(pagecache_division_limit, pagecache_division_limit_alias,
       PLUGIN_VAR_RQCMDARG,
       "Deprecated, use --aria-pagecache-division-limit instead",
       0, 0, 100,  1, 100, 1);

ulong recover_alias;
static MYSQL_SYSVAR_ENUM(recover, recover_alias, PLUGIN_VAR_OPCMDARG,
       "Deprecated, use --aria-recover instead",
       NULL, NULL, HA_RECOVER_DEFAULT, &maria_recover_typelib);

ulong repair_threads_alias;
static MYSQL_THDVAR_ULONG(repair_threads, PLUGIN_VAR_RQCMDARG,
       "Deprecated, use --aria-repair-threads instead",
       0, 0, 1, 1, ~0L, 1);

ulong sort_buffer_size_alias;
static MYSQL_THDVAR_ULONG(sort_buffer_size, PLUGIN_VAR_RQCMDARG,
       "Deprecated, use --aria-sort-buffer-size instead",
       0, 0, 128L*1024L*1024L, 4, ~0L, 1);

ulong stats_method_alias;
static MYSQL_THDVAR_ENUM(stats_method, PLUGIN_VAR_RQCMDARG,
       "Deprecated, use --aria-stats-method instead",
       0, 0, 0, &maria_stats_method_typelib);

ulong sync_log_dir_alias;
static MYSQL_SYSVAR_ENUM(sync_log_dir, sync_log_dir_alias,
       PLUGIN_VAR_RQCMDARG,
       "Deprecated, use --aria-sync-log-dir instead",
       NULL, NULL, TRANSLOG_SYNC_DIR_NEWFILE,
       &maria_sync_log_dir_typelib);

my_bool used_for_temp_tables_alias= 1;
static MYSQL_SYSVAR_BOOL(used_for_temp_tables, 
       used_for_temp_tables_alias, PLUGIN_VAR_READONLY | PLUGIN_VAR_NOCMDOPT,
       NULL, 0, 0, 1);

static struct st_mysql_show_var status_variables_aliases[]= {
  {"Maria", (char*) &status_variables, SHOW_ARRAY},
  {NullS, NullS, SHOW_LONG}
};

/*
  There is one problem with aliases for command-line options.
  Plugin initialization works like this

     for all plugins:
       prepare command-line options
       initialize command-line option variables to the default values
       parse command line, assign values as necessary

     for all plugins:
       call the plugin initialization function

  it means, we cannot have maria* and aria* command-line options to use
  the same underlying variables - because after assigning maria* values,
  MySQL will put there default values again preparing for parsing aria*
  values. So, maria* values will be lost.

  So, we create separate set of variables for maria* options,
  and take both values into account in ha_maria_init().

  When the command line was parsed, we patch maria* options
  to use the same variables as aria* options so that
  set @@maria_some_var would have the same value as @@aria_some_var
  without forcing us to copy the values around all the time.
*/

static struct st_mysql_sys_var* system_variables_aliases[]= {
  MYSQL_SYSVAR(block_size),
  MYSQL_SYSVAR(checkpoint_interval),
  MYSQL_SYSVAR(force_start_after_recovery_failures),
  MYSQL_SYSVAR(group_commit),
  MYSQL_SYSVAR(group_commit_interval),
  MYSQL_SYSVAR(log_dir_path),
  MYSQL_SYSVAR(log_file_size),
  MYSQL_SYSVAR(log_purge_type),
  MYSQL_SYSVAR(max_sort_file_size),
  MYSQL_SYSVAR(page_checksum),
  MYSQL_SYSVAR(pagecache_age_threshold),
  MYSQL_SYSVAR(pagecache_buffer_size),
  MYSQL_SYSVAR(pagecache_division_limit),
  MYSQL_SYSVAR(recover),
  MYSQL_SYSVAR(repair_threads),
  MYSQL_SYSVAR(sort_buffer_size),
  MYSQL_SYSVAR(stats_method),
  MYSQL_SYSVAR(sync_log_dir),
  MYSQL_SYSVAR(used_for_temp_tables),
  NULL
};

#define COPY_SYSVAR(name) \
  memcpy(&MYSQL_SYSVAR_NAME(name), system_variables[i++],                 \
                                        sizeof(MYSQL_SYSVAR_NAME(name))); \
  if (name ## _alias  != MYSQL_SYSVAR_NAME(name).def_val &&               \
      *MYSQL_SYSVAR_NAME(name).value == MYSQL_SYSVAR_NAME(name).def_val)  \
    *MYSQL_SYSVAR_NAME(name).value= name ## _alias;

#define COPY_THDVAR(name) \
  name ## _alias= THDVAR(0, name);                                        \
  memcpy(&MYSQL_SYSVAR_NAME(name), system_variables[i++],                 \
                                        sizeof(MYSQL_SYSVAR_NAME(name))); \
  if (name ## _alias  != MYSQL_SYSVAR_NAME(name).def_val &&               \
      THDVAR(0, name) == MYSQL_SYSVAR_NAME(name).def_val)                 \
    THDVAR(0, name)= name ## _alias;

void copy_variable_aliases()
{
  int i= 0;
  COPY_SYSVAR(block_size);
  COPY_SYSVAR(checkpoint_interval);
  COPY_SYSVAR(force_start_after_recovery_failures);
  COPY_SYSVAR(group_commit);
  COPY_SYSVAR(group_commit_interval);
  COPY_SYSVAR(log_dir_path);
  COPY_SYSVAR(log_file_size);
  COPY_SYSVAR(log_purge_type);
  COPY_SYSVAR(max_sort_file_size);
  COPY_SYSVAR(page_checksum);
  COPY_SYSVAR(pagecache_age_threshold);
  COPY_SYSVAR(pagecache_buffer_size);
  COPY_SYSVAR(pagecache_division_limit);
  COPY_SYSVAR(recover);
  COPY_THDVAR(repair_threads);
  COPY_THDVAR(sort_buffer_size);
  COPY_THDVAR(stats_method);
  COPY_SYSVAR(sync_log_dir);
  COPY_SYSVAR(used_for_temp_tables);
}

struct st_maria_plugin compat_aliases= {
  MYSQL_DAEMON_PLUGIN,
  &maria_storage_engine,
  "Maria",
  "Monty Program Ab",
  "Compatibility aliases for the Aria engine",
  PLUGIN_LICENSE_GPL,
  NULL,
  NULL,
  0x0105,
  status_variables_aliases,
  system_variables_aliases,
  "1.5",
  MariaDB_PLUGIN_MATURITY_GAMMA
};

