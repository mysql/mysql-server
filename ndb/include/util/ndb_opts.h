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

#include <my_sys.h>
#include <my_getopt.h>
#include <mysql_version.h>
#include <ndb_version.h>

#ifndef DBUG_OFF
#define NDB_STD_OPTS(prog_name) \
  { "debug", '#', "Output debug log. Often this is 'd:t:o,filename'.", \
    0, 0, 0, GET_STR, OPT_ARG, 0, 0, 0, 0, 0, 0 }, \
  { "usage", '?', "Display this help and exit.", \
    0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0 }, \
  { "help", '?', "Display this help and exit.", \
    0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0 }, \
  { "version", 'V', "Output version information and exit.", 0, 0, 0, \
    GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0 }, \
  { "connect-string", 'c', \
    "Set connect string for connecting to ndb_mgmd. " \
    "<constr>=\"host=<hostname:port>[;nodeid=<id>]\". " \
    "Overides specifying entries in NDB_CONNECTSTRING and config file", \
    (gptr*) &opt_connect_str, (gptr*) &opt_connect_str, 0, \
    GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 }
#else
#define NDB_STD_OPTS(prog_name) \
  { "usage", '?', "Display this help and exit.", \
    0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0 }, \
  { "help", '?', "Display this help and exit.", \
    0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0 }, \
  { "version", 'V', "Output version information and exit.", 0, 0, 0, \
    GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0 }, \
  { "connect-string", 'c', \
    "Set connect string for connecting to ndb_mgmd. " \
    "<constr>=\"host=<hostname:port>[;nodeid=<id>]\". " \
    "Overides specifying entries in NDB_CONNECTSTRING and config file", \
    (gptr*) &opt_connect_str, (gptr*) &opt_connect_str, 0, \
    GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 }
#endif

#endif /*_NDB_OPTS_H */
