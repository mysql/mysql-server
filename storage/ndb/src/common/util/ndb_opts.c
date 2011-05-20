/*
   Copyright (c) 2008, 2010, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#define OPTEXPORT
#include <ndb_opts.h>

#include <ndb_version.h>


static void default_ndb_opt_short(void)
{
  ndb_short_usage_sub(NULL);
}

static void default_ndb_opt_usage(void)
{
  struct my_option my_long_options[] =
    {
      NDB_STD_OPTS("ndbapi_program")
    };
  const char *load_default_groups[]= { "mysql_cluster", 0 };

  ndb_usage(default_ndb_opt_short, load_default_groups, my_long_options);
}

static void (*g_ndb_opt_short_usage)(void)= default_ndb_opt_short;
static void (*g_ndb_opt_usage)(void)= default_ndb_opt_usage;

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

void ndb_short_usage_sub(const char* extra)
{
  printf("Usage: %s [OPTIONS]%s%s\n", ndb_progname(),
         (extra)?" ":"",
         (extra)?extra:"");
}

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


my_bool
ndb_std_get_one_option(int optid,
                       const struct my_option *opt __attribute__((unused)),
                       char *argument __attribute__((unused)))
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

my_bool ndb_is_load_default_arg_separator(const char* arg)
{
#ifndef MYSQL_VERSION_ID
#error "Need MYSQL_VERSION_ID defined"
#endif

#if MYSQL_VERSION_ID >= 50510
  /*
    load_default() in 5.5+ returns an extra arg which has to
    be skipped when processing the argv array
   */
  if (my_getopt_is_args_separator(arg))
    return TRUE;
#elif MYSQL_VERSION_ID >= 50501
  if (arg == args_separator)
    return TRUE;
#else
  (void)arg;
#endif
  return FALSE;
}

