#ifndef SQL_QUERY_REWRITE_INCLUDED
#define SQL_QUERY_REWRITE_INCLUDED

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include <mysql/plugin_audit.h>

/**
  Calls the query rewrite plugins' respective rewrite functions before parsing
  the query.

  @param[in] thd The session sending the query to be rewritten.
*/
void invoke_pre_parse_rewrite_plugins(THD *thd);

/**
  Enables digests in the parser state if any plugin needs it.

  @param param ps This parser state will have digests enabled if any plugin
  needs it.

  @note For the time being, only post-parse query rewrite plugins are able to
  request digests. If other plugin types need the same, this function needs to
  be modified.
*/
void enable_digest_if_any_plugin_needs_it(THD *thd, Parser_state *ps);

/**
  Calls query rewrite plugins after parsing the query.
  @param[in] thd the thread with the query to be rewritten
*/
bool invoke_post_parse_rewrite_plugins(THD *thd, my_bool is_prepared);

#endif /* SQL_QUERY_REWRITE_INCLUDED */
