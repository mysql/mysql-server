/*
   Copyright (c) 2008, 2019, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#define OPTEXPORT
#include <ndb_opts.h>

#include <ndb_version.h>
#include "my_alloc.h"
#include "my_default.h"

static const char *load_default_groups[]= { "mysql_cluster", 0 };

static void default_ndb_opt_short(void)
{
  ndb_short_usage_sub(NULL);
}

extern "C"     /* declaration only */
void ndb_usage(void (*usagefunc)(void), const char *load_default_groups[],
               struct my_option *my_long_options);

static void default_ndb_opt_usage(void)
{
  struct my_option my_long_options[] =
    {
      NDB_STD_OPTS("ndbapi_program")
    };

  ndb_usage(default_ndb_opt_short, load_default_groups, my_long_options);
}

static void (*g_ndb_opt_short_usage)(void)= default_ndb_opt_short;
static void (*g_ndb_opt_usage)(void)= default_ndb_opt_usage;

extern "C"
void ndb_opt_set_usage_funcs(void (*short_usage)(void),
                             void (*usage)(void))
{
  /* Check that the program name has been set already */
  assert(my_progname);

  if(short_usage)
    g_ndb_opt_short_usage= short_usage;
  if(usage)
    g_ndb_opt_usage= usage;
}

static inline
const char* ndb_progname(void)
{
  if (my_progname)
    return my_progname;
  return "<unknown program>";
}

extern "C"
void ndb_short_usage_sub(const char* extra)
{
  printf("Usage: %s [OPTIONS]%s%s\n", ndb_progname(),
         (extra)?" ":"",
         (extra)?extra:"");
}

extern "C"
void ndb_usage(void (*usagefunc)(void), const char *load_default_groups[],
               struct my_option *my_long_options)
{
  (*usagefunc)();

  ndb_std_print_version();
  print_defaults(MYSQL_CONFIG_NAME,load_default_groups);
  puts("");
  my_print_help(my_long_options);
  my_print_variables(my_long_options);
}

static
void empty_long_usage_extra_func()
{
}

extern "C"
bool
ndb_std_get_one_option(int optid,
                       const struct my_option *opt MY_ATTRIBUTE((unused)),
                       char *argument MY_ATTRIBUTE((unused)))
{
  switch (optid) {
#ifndef DBUG_OFF
  case '#':
    if (!opt_debug)
      opt_debug= "d:t";
    DBUG_SET_INITIAL(argument ? argument : opt_debug);
    opt_ndb_endinfo= 1;
    break;
#endif
  case 'V':
    ndb_std_print_version();
    exit(0);
  case '?':
    (*g_ndb_opt_usage)();
    exit(0);
  }
  return 0;
}

extern "C"
void ndb_std_print_version()
{
#ifndef DBUG_OFF
  const char *suffix= "-debug";
#else
  const char *suffix= "";
#endif
  printf("MySQL distrib %s%s, for %s (%s)\n",
         NDB_VERSION_STRING,suffix,SYSTEM_TYPE,MACHINE_TYPE);
}

extern "C"
bool ndb_is_load_default_arg_separator(const char* arg)
{
  /*
    load_default() in 5.5+ returns an extra arg which has to
    be skipped when processing the argv array
   */
  if (my_getopt_is_args_separator(arg))
    return TRUE;
  return FALSE;
}

static Ndb_opts * registeredNdbOpts;

static void ndb_opts_usage()
{
  registeredNdbOpts->usage();
}

void
Ndb_opts::registerUsage(Ndb_opts *r)
{
  assert(registeredNdbOpts == NULL);
  registeredNdbOpts = r;
  ndb_opt_set_usage_funcs(default_ndb_opt_short, ndb_opts_usage);
}

void Ndb_opts::release()
{
  registeredNdbOpts = NULL;
}

Ndb_opts::Ndb_opts(int & argc_ref, char** & argv_ref,
                   struct my_option * long_options,
                   const char * default_groups[])
:
  opts_mem_root(),
  main_argc_ptr(& argc_ref),
  main_argv_ptr(& argv_ref),
  mycnf_default_groups(default_groups ? default_groups : load_default_groups),
  options(long_options),
  short_usage_fn(g_ndb_opt_short_usage)
{
  my_load_defaults(MYSQL_CONFIG_NAME,  mycnf_default_groups,
                   main_argc_ptr, main_argv_ptr,  &opts_mem_root, NULL);
  Ndb_opts::registerUsage(this);
}

Ndb_opts::~Ndb_opts()
{
  Ndb_opts::release();
}

int Ndb_opts::handle_options(bool (*get_opt_fn)
                             (int, const struct my_option *, char *)) const
{
  return ::handle_options(main_argc_ptr, main_argv_ptr, options, get_opt_fn);
}

void Ndb_opts::set_usage_funcs(void (*short_fn)(void),
                               void (*long_fn)(void))
{
  short_usage_fn = short_fn;
  long_usage_extra_fn = long_fn ? long_fn : empty_long_usage_extra_func;
}

void Ndb_opts::usage() const
{
  long_usage_extra_fn();
  ndb_usage(short_usage_fn, mycnf_default_groups, options);
}

