/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#ifndef _NDB_OPTS_H
#define _NDB_OPTS_H

#include <ndb_global.h>
#include <my_sys.h>
#include <my_getopt.h>
#include <mysql_version.h>
#include <ndb_version.h>

#define NDB_STD_OPTS_VARS \
const char *opt_connect_str= 0;\
my_bool opt_ndb_shm;\
my_bool	opt_ndb_optimized_node_selection

#define NDB_STD_OPTS_OPTIONS \
OPT_NDB_SHM= 256,\
OPT_NDB_OPTIMIZED_NODE_SELECTION

#define OPT_NDB_CONNECTSTRING 'c'

#ifdef NDB_SHM_TRANSPORTER
#define OPT_NDB_SHM_DEFAULT 1
#else
#define OPT_NDB_SHM_DEFAULT 0
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
    "Overides specifying entries in NDB_CONNECTSTRING and Ndb.cfg", \
    (gptr*) &opt_connect_str, (gptr*) &opt_connect_str, 0, \
    GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 },\
  { "ndb-shm", OPT_NDB_SHM,\
    "Allow optimizing using shared memory connections when available",\
    (gptr*) &opt_ndb_shm, (gptr*) &opt_ndb_shm, 0,\
    GET_BOOL, NO_ARG, OPT_NDB_SHM_DEFAULT, 0, 0, 0, 0, 0 },\
  {"ndb-optimized-node-selection", OPT_NDB_OPTIMIZED_NODE_SELECTION,\
    "Select nodes for transactions in a more optimal way",\
    (gptr*) &opt_ndb_optimized_node_selection,\
    (gptr*) &opt_ndb_optimized_node_selection, 0,\
    GET_BOOL, OPT_ARG, 1, 0, 0, 0, 0, 0},\
  { "connect-string", OPT_NDB_CONNECTSTRING, "same as --ndb-connectstring",\
    (gptr*) &opt_connect_str, (gptr*) &opt_connect_str, 0,\
    GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 }

#ifndef DBUG_OFF
#define NDB_STD_OPTS(prog_name) \
  { "debug", '#', "Output debug log. Often this is 'd:t:o,filename'.", \
    0, 0, 0, GET_STR, OPT_ARG, 0, 0, 0, 0, 0, 0 }, \
  NDB_STD_OPTS_COMMON
#else
#define NDB_STD_OPTS(prog_name) NDB_STD_OPTS_COMMON
#endif

#endif /*_NDB_OPTS_H */
