/*
   Copyright (c) 2004, 2017, Oracle and/or its affiliates. All rights reserved.

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

#ifndef _NDB_OPTS_H
#define _NDB_OPTS_H

#include <ndb_global.h>

#include <my_sys.h> /* loglevel needed by my_getopt.h */
#include <my_getopt.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef OPTEXPORT
#define OPT_EXTERN(T,V,I) T V I
#else
#define OPT_EXTERN(T,V,I) extern T V
#endif

#define NONE
OPT_EXTERN(int,opt_ndb_nodeid,NONE);
OPT_EXTERN(bool,opt_ndb_endinfo,=0);
OPT_EXTERN(bool,opt_core,NONE);
OPT_EXTERN(bool,opt_ndb_optimized_node_selection,NONE);
OPT_EXTERN(const char *,opt_ndb_connectstring,=0);
OPT_EXTERN(int, opt_connect_retry_delay,NONE);
OPT_EXTERN(int, opt_connect_retries,NONE);

#ifndef DBUG_OFF
OPT_EXTERN(const char *,opt_debug,= 0);
#endif

#if defined VM_TRACE
#define OPT_WANT_CORE_DEFAULT 1
#else
#define OPT_WANT_CORE_DEFAULT 0
#endif

#define NDB_STD_OPTS_COMMON \
  { "usage", '?', "Display this help and exit.", \
    0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0 }, \
  { "help", '?', "Display this help and exit.", \
    0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0 }, \
  { "version", 'V', "Output version information and exit.", 0, 0, 0, \
    GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0 }, \
  { "ndb-connectstring", OPT_NDB_CONNECTSTRING, \
    "Set connect string for connecting to ndb_mgmd. " \
    "Syntax: \"[nodeid=<id>;][host=]<hostname>[:<port>]\". " \
    "Overrides specifying entries in NDB_CONNECTSTRING and my.cnf", \
    (uchar**) &opt_ndb_connectstring, (uchar**) &opt_ndb_connectstring, \
    0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 },\
  { "ndb-mgmd-host", NDB_OPT_NOSHORT, \
    "same as --ndb-connectstring", \
    (uchar**) &opt_ndb_connectstring, (uchar**) &opt_ndb_connectstring, 0, \
    GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 },\
  { "ndb-nodeid", NDB_OPT_NOSHORT, \
    "Set node id for this node. Overrides node id specified " \
    "in --ndb-connectstring.", \
    (uchar**) &opt_ndb_nodeid, (uchar**) &opt_ndb_nodeid, 0, \
    GET_INT, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 },\
  {"ndb-optimized-node-selection", NDB_OPT_NOSHORT,\
    "Select nodes for transactions in a more optimal way",\
    (uchar**) &opt_ndb_optimized_node_selection,\
    (uchar**) &opt_ndb_optimized_node_selection, 0,\
    GET_BOOL, OPT_ARG, 1, 0, 0, 0, 0, 0},\
  { "connect-string", OPT_NDB_CONNECTSTRING, "same as --ndb-connectstring",\
    (uchar**) &opt_ndb_connectstring, (uchar**) &opt_ndb_connectstring, \
    0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 },\
  { "core-file", NDB_OPT_NOSHORT, "Write core on errors.",\
    (uchar**) &opt_core, (uchar**) &opt_core, 0,\
    GET_BOOL, NO_ARG, OPT_WANT_CORE_DEFAULT, 0, 0, 0, 0, 0},\
  {"character-sets-dir", NDB_OPT_NOSHORT,\
     "Directory where character sets are.", (uchar**) &charsets_dir,\
     (uchar**) &charsets_dir, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},\
  {"connect-retry-delay", NDB_OPT_NOSHORT, \
     "Set connection time out." \
     " This is the number of seconds after which the tool tries" \
     " reconnecting to the cluster.", \
     (uchar**) &opt_connect_retry_delay, (uchar**) &opt_connect_retry_delay, 0, GET_INT, \
     REQUIRED_ARG, 5, 1, INT_MAX, 0, 0, 0},\
  {"connect-retries", NDB_OPT_NOSHORT, \
     "Set connection retries." \
     " This is the number of times the tool tries connecting" \
     " to the cluster.", \
     (uchar**) &opt_connect_retries, (uchar**) &opt_connect_retries, 0, GET_INT, \
     REQUIRED_ARG, 12, 0, INT_MAX, 0, 0, 0}

#ifndef DBUG_OFF
#define NDB_STD_OPTS(prog_name) \
  { "debug", '#', "Output debug log. Often this is 'd:t:o,filename'.", \
    (uchar**) &opt_debug, (uchar**) &opt_debug, \
    0, GET_STR, OPT_ARG, 0, 0, 0, 0, 0, 0 }, \
  NDB_STD_OPTS_COMMON
#else
#define NDB_STD_OPTS(prog_name) NDB_STD_OPTS_COMMON
#endif

void ndb_std_print_version();

enum ndb_std_options {
  /*
    --ndb-connectstring=<connectstring> has short form 'c'
  */
  OPT_NDB_CONNECTSTRING = 'c',

  /*
    For arguments that have neither a short form option or need
    special processing in 'get_one_option' callback
  */
  NDB_OPT_NOSHORT = 256,

 /*
   should always be last in this enum and will be used as the
   start value by programs which use 'ndb_std_get_one_option' and
   need to define their own arguments with special processing
 */
  NDB_STD_OPTIONS_LAST
};

void ndb_opt_set_usage_funcs(void (*short_usage)(void),
                             void (*usage)(void));
bool
ndb_std_get_one_option(int optid,
		       const struct my_option *opt MY_ATTRIBUTE((unused)),
                       char *argument);

void ndb_usage(void (*usagefunc)(void), const char *load_default_groups[],
               struct my_option *my_long_options);
void ndb_short_usage_sub(const char* extra);

bool ndb_is_load_default_arg_separator(const char* arg);

/* Read the given [groups] from <conf_file> and return in argc/argv */
int ndb_load_defaults(const char* conf_file,
                      const char** groups,
                      int *argc, char*** argv);
/* Free memory returned in "argv" from ndb_load_defaults() */
void ndb_free_defaults(char** argv);

#ifdef __cplusplus
}
#endif

#endif /*_NDB_OPTS_H */
