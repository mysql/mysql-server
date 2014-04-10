/* Copyright (c) 2012, 2014, Oracle and/or its affiliates. All rights reserved.

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


#include "my_global.h"

C_MODE_START

extern const char *my_defaults_extra_file;
extern const char *my_defaults_group_suffix;
extern const char *my_defaults_file;
extern my_bool my_getopt_use_args_separator;

/* Define the type of function to be passed to process_default_option_files */
typedef int (*Process_option_func)(void *ctx, const char *group_name,
                                   const char *option);

my_bool my_getopt_is_args_separator(const char* arg);
int get_defaults_options(int argc, char **argv,
                         char **defaults, char **extra_defaults,
                         char **group_suffix, char **login_path);
int my_load_defaults(const char *conf_file, const char **groups,
                     int *argc, char ***argv, const char ***);
int check_file_permissions(const char *file_name);
int load_defaults(const char *conf_file, const char **groups,
                  int *argc, char ***argv);
int my_search_option_files(const char *conf_file, int *argc,
                           char ***argv, uint *args_used,
                           Process_option_func func, void *func_ctx,
                           const char **default_directories);
void free_defaults(char **argv);
void my_print_default_files(const char *conf_file);
void print_defaults(const char *conf_file, const char **groups);

C_MODE_END

#endif  // MY_DEFAULT_INCLUDED
