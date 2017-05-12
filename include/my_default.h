/* Copyright (c) 2012, 2017, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */

#ifndef MY_DEFAULT_INCLUDED
#define MY_DEFAULT_INCLUDED

/**
  @file include/my_default.h
*/

#include <sys/types.h>

#include "my_inttypes.h"
#include "my_macros.h"

C_MODE_START

extern const char *my_defaults_extra_file;
extern const char *my_defaults_group_suffix;
extern const char *my_defaults_file;
extern bool my_getopt_use_args_separator;
extern bool my_defaults_read_login_file;
extern bool no_defaults;

/* Define the type of function to be passed to process_default_option_files */
typedef int (*Process_option_func)(void *ctx, const char *group_name,
                                   const char *option, const char *cnf_file);

bool my_getopt_is_args_separator(const char* arg);
int get_defaults_options(int argc, char **argv,
                         char **defaults, char **extra_defaults,
                         char **group_suffix, char **login_path,
                         bool found_no_defaults);
int my_load_defaults(const char *conf_file, const char **groups,
                     int *argc, char ***argv, const char ***);
int check_file_permissions(const char *file_name, bool is_login_file);
int load_defaults(const char *conf_file, const char **groups,
                  int *argc, char ***argv);
int my_search_option_files(const char *conf_file, int *argc,
                           char ***argv, uint *args_used,
                           Process_option_func func, void *func_ctx,
                           const char **default_directories,
                           bool is_login_file, bool found_no_defaults);
void free_defaults(char **argv);
void my_print_default_files(const char *conf_file);
void print_defaults(const char *conf_file, const char **groups);
void init_variable_default_paths();
void update_variable_source(const char* opt_name, const char* config_file);
void set_variable_source(const char *opt_name, void* value);

C_MODE_END

#endif  // MY_DEFAULT_INCLUDED
