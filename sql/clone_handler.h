/* Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/**
@file clone_handler.h
Clone handler interface to access clone plugin
*/

#ifndef CLONE_HANDLER_INCLUDED
#define CLONE_HANDLER_INCLUDED

#include <string>

#include "my_io.h"
#include "sql/sql_plugin_ref.h"  // plugin_ref

class THD;
struct Mysql_clone;

/**
  Clone plugin handler to convenient way to. Takes
*/
class Clone_handler {
 public:
  /** Constructor: Initialize plugin name */
  Clone_handler(const char *plugin_name_arg) : m_plugin_handle(nullptr) {
    m_plugin_name.assign(plugin_name_arg);
  }

  /** Initialize plugin handle
  @return error code */
  int init();

  /** Clone handler interface for local clone.
  @param[in]	thd		server thread handle
  @param[in]	data_dir	cloned data directory
  @return error code */
  int clone_local(THD *thd, const char *data_dir);

  /** Clone handler interface for remote clone client.
  @param[in]	thd		server thread handle
  @param[in]	data_dir	cloned data directory
  @return error code */
  int clone_remote_client(THD *thd, const char *data_dir);

  /** Clone handler interface for remote clone server.
  @param[in]	thd	server thread handle
  @param[in]	socket	network socket to remote client
  @return error code */
  int clone_remote_server(THD *thd, my_socket socket);

 private:
  /** Validate clone data directory and convert to os format
  @param[in]	in_dir	user specified clone directory
  @param[out]	out_dir	data directory in native os format
  @return error code */
  int validate_dir(const char *in_dir, char *out_dir);

  /** Clone plugin name */
  std::string m_plugin_name;

  /** Clone plugin handle */
  Mysql_clone *m_plugin_handle;
};

/** Check if the clone plugin is installed and lock. If the plugin is ready,
return the handler to caller.
@param[in]	thd	server thread handle
@param[out]	plugin	plugin reference
@return clone handler on success otherwise NULL */
Clone_handler *clone_plugin_lock(THD *thd, plugin_ref *plugin);

/** Unlock the clone plugin.
@param[in]	thd	server thread handle
@param[out]	plugin	plugin reference */
void clone_plugin_unlock(THD *thd, plugin_ref plugin);

#endif /* CLONE_HANDLER_INCLUDED */
