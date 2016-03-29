/* Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved.

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

#ifndef MYSQLD_EMBEDDED_INCLUDED
#define MYSQLD_EMBEDDED_INCLUDED

// Declarations only needed by embedded server. Definitions in mysqld.cc

#include "my_global.h"

extern int defaults_argc;
extern char **defaults_argv;
extern int remaining_argc;
extern char **remaining_argv;
extern bool opt_endinfo;
extern size_t mysql_real_data_home_len;

void clean_up(bool print_message);

int init_ssl();

int init_server_components();

int init_server_auto_options();

bool read_init_file(char *file_name);

void server_components_initialized();

int handle_early_options();

void adjust_related_options(ulong *requested_open_files);

#endif // MYSQLD_EMBEDDED_INCLUDED
