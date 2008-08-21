/* Copyright (C) 2003 MySQL AB

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

#ifndef _NDB_OPTS_H
#define _NDB_OPTS_H

#ifdef __cplusplus
extern "C" {
#endif
#include <ndb_global.h>
#include <my_sys.h>
#include <my_getopt.h>
#include <mysql_version.h>
#include <ndb_version.h>
#include <ndbapi/ndb_opt_defaults.h>

#ifdef OPTEXPORT
#define OPT_EXTERN(T,V,I) T V I
#else
#define OPT_EXTERN(T,V,I) extern T V
#endif

#define NONE
OPT_EXTERN(int,opt_ndb_nodeid,NONE);
OPT_EXTERN(my_bool,opt_endinfo,=0);
OPT_EXTERN(my_bool,opt_ndb_shm,NONE);
OPT_EXTERN(my_bool,opt_core,NONE);
OPT_EXTERN(my_bool,opt_ndb_optimized_node_selection,NONE);
OPT_EXTERN(char *,opt_ndb_connectstring,=0);
OPT_EXTERN(const char *,opt_connect_str,=0);
OPT_EXTERN(const char *,opt_ndb_mgmd,=0);
OPT_EXTERN(char,opt_ndb_constrbuf[1024],NONE);
OPT_EXTERN(unsigned,opt_ndb_constrbuf_len,=0);

#ifndef DBUG_OFF
OPT_EXTERN(const char *,opt_debug,= 0);
#endif

#define OPT_NDB_CONNECTSTRING 'c'
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
  { "ndb-mgmd-host", OPT_NDB_MGMD, \
    "Set host and port for connecting to ndb_mgmd. " \
    "Syntax: <hostname>[:<port>].", \
    (uchar**) &opt_ndb_mgmd, (uchar**) &opt_ndb_mgmd, 0, \
    GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 },\
  { "ndb-nodeid", OPT_NDB_NODEID, \
    "Set node id for this node.", \
    (uchar**) &opt_ndb_nodeid, (uchar**) &opt_ndb_nodeid, 0, \
    GET_INT, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 },\
  { "ndb-shm", OPT_NDB_SHM,\
    "Allow optimizing using shared memory connections when available",\
    (uchar**) &opt_ndb_shm, (uchar**) &opt_ndb_shm, 0,\
    GET_BOOL, NO_ARG, OPT_NDB_SHM_DEFAULT, 0, 0, 0, 0, 0 },\
  {"ndb-optimized-node-selection", OPT_NDB_OPTIMIZED_NODE_SELECTION,\
    "Select nodes for transactions in a more optimal way",\
    (uchar**) &opt_ndb_optimized_node_selection,\
    (uchar**) &opt_ndb_optimized_node_selection, 0,\
    GET_BOOL, OPT_ARG, 1, 0, 0, 0, 0, 0},\
  { "connect-string", OPT_NDB_CONNECTSTRING, "same as --ndb-connectstring",\
    (uchar**) &opt_ndb_connectstring, (uchar**) &opt_ndb_connectstring, \
    0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 },\
  { "core-file", OPT_WANT_CORE, "Write core on errors.",\
    (uchar**) &opt_core, (uchar**) &opt_core, 0,\
    GET_BOOL, NO_ARG, OPT_WANT_CORE_DEFAULT, 0, 0, 0, 0, 0},\
  {"character-sets-dir", OPT_CHARSETS_DIR,\
     "Directory where character sets are.", (uchar**) &charsets_dir,\
     (uchar**) &charsets_dir, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0}\

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

static void usage();

enum ndb_std_options {
  OPT_NDB_SHM= 256,
  OPT_NDB_SHM_SIGNUM,
  OPT_NDB_OPTIMIZED_NODE_SELECTION,
  OPT_WANT_CORE,
  OPT_NDB_MGMD,
  OPT_NDB_NODEID,
  OPT_CHARSETS_DIR,
  NDB_STD_OPTIONS_LAST /* should always be last in this enum */
};

my_bool
ndb_std_get_one_option(int optid,
		       const struct my_option *opt __attribute__((unused)),
                       char *argument);

void ndb_usage();
void ndb_short_usage_sub();
#ifdef PROVIDE_USAGE
inline void usage(){ndb_usage();}
inline void short_usage_sub(){ndb_short_usage_sub();}
#endif

#ifdef __cplusplus
}
#endif
#endif /*_NDB_OPTS_H */
