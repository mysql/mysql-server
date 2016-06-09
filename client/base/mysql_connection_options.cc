/*
   Copyright (c) 2014, 2016, Oracle and/or its affiliates. All rights reserved.

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

#include <sstream>
#include <vector>
#include "abstract_options_provider.h"
#include "mysql_connection_options.h"
#include "abstract_program.h"
#include <mysys_err.h>

using Mysql::Tools::Base::Abstract_program;
using namespace Mysql::Tools::Base::Options;
using Mysql::Nullable;
using std::vector;
using std::string;

bool Mysql_connection_options::mysql_inited;

static void atexit_mysql_library_end()
{
  mysql_library_end();
}

Mysql_connection_options::Mysql_connection_options(Abstract_program *program)
  : m_ssl_options_provider(),
    m_program(program),
    m_protocol(0)
{
  if (Mysql_connection_options::mysql_inited == false)
  {
    Mysql_connection_options::mysql_inited= true;
    mysql_library_init(0, NULL, NULL);
    atexit(atexit_mysql_library_end);
  }

  this->add_provider(&this->m_ssl_options_provider);
}

void Mysql_connection_options::create_options()
{
  this->create_new_option(&this->m_bind_addr, "bind-address",
    "IP address to bind to.");
  this->create_new_option((char**)&charsets_dir, "character-sets-dir",
    "Directory for character set files.");
  this->create_new_option(&this->m_compress, "compress",
      "Use compression in server/client protocol.")
    ->set_short_character('C');
  this->create_new_option(&this->m_default_charset, "default-character-set",
    "Set the default character set.")
    ->set_value("UTF8MB4");
  this->create_new_option(&this->m_host, "host", "Connect to host.")
    ->set_short_character('h');
  this->create_new_option(&this->m_max_allowed_packet, "max_allowed_packet",
      "The maximum packet length to send to or receive from server.")
    ->set_minimum_value(4096)
    ->set_maximum_value(2U*1024*1024*1024)
    ->set_value_step(1024)
    ->set_value(24*1024*1024);
  this->create_new_option(&this->m_net_buffer_length, "net_buffer_length",
      "The buffer size for TCP/IP and socket communication.")
    ->set_minimum_value(4096)
    ->set_maximum_value(16*1024*1024)
    ->set_value_step(1024)
    ->set_value(1024*1024L-1024);
  this->create_new_password_option(&this->m_password, "password",
      "Password to use when connecting to server. If password is not given,"
      " it's solicited on the tty.")
    ->set_short_character('p');
#ifdef _WIN32
  this->create_new_option("pipe", "Use named pipes to connect to server.")
    ->set_short_character('W')
    ->add_callback(new Instance_callback<void, char*, Mysql_connection_options>
      (this, &Mysql_connection_options::pipe_protocol_callback));
#endif
  this->create_new_option(&this->m_mysql_port, "port",
      "Port number to use for connection.")
    ->set_short_character('P');
  this->create_new_option(&this->m_protocol_string, "protocol",
      "The protocol to use for connection (tcp, socket, pipe, memory).")
    ->add_callback(new Instance_callback<void, char*, Mysql_connection_options>
    (this, &Mysql_connection_options::protocol_callback));
#if defined (_WIN32) && !defined (EMBEDDED_LIBRARY)
  this->create_new_option(&this->m_shared_memory_base_name,
    "shared-memory-base-name", "Base name of shared memory.");
#endif
  this->create_new_option(&this->m_mysql_unix_port, "socket",
    "The socket file to use for connection.")
    ->set_short_character('S');
  this->create_new_option(&this->m_secure_auth, "secure-auth",
      "Refuse client connecting to server if it uses old (pre-4.1.1) "
      "protocol. Deprecated. Always TRUE")
    ->add_callback(new Instance_callback<void, char*, Mysql_connection_options>
      (this, &Mysql_connection_options::secure_auth_callback));
  this->create_new_option(&this->m_user, "user",
    "User for login if not current user.")
    ->set_short_character('u');
  this->create_new_option(&this->m_plugin_dir, "plugin_dir",
    "Directory for client-side plugins.");
  this->create_new_option(&this->m_default_auth, "default_auth",
    "Default authentication client-side plugin to use.");
}

MYSQL* Mysql_connection_options::create_connection()
{
  MYSQL *connection= mysql_init(NULL);
  if (this->m_compress)
    mysql_options(connection, MYSQL_OPT_COMPRESS, NullS);

  this->m_ssl_options_provider.apply_for_connection(connection);

  if (this->m_protocol)
    mysql_options(connection,MYSQL_OPT_PROTOCOL,
      (char*)&this->m_protocol);
  if (this->m_bind_addr.has_value())
    mysql_options(connection,MYSQL_OPT_BIND,
      this->m_bind_addr.value().c_str());
  if (!this->m_secure_auth)
    mysql_options(connection,MYSQL_SECURE_AUTH,
      (char*)&this->m_secure_auth);
#if defined (_WIN32) && !defined (EMBEDDED_LIBRARY)
  if (this->m_shared_memory_base_name.has_value())
    mysql_options(connection,MYSQL_SHARED_MEMORY_BASE_NAME,
      this->m_shared_memory_base_name.value().c_str());
#endif
  if (this->m_default_charset.has_value())
  {
    mysql_options(connection, MYSQL_SET_CHARSET_NAME,
      this->m_default_charset.value().c_str());
  }
  else
  {
    mysql_options(connection, MYSQL_SET_CHARSET_NAME, "utf8mb4");
  }
  if (this->m_plugin_dir.has_value())
    mysql_options(connection, MYSQL_PLUGIN_DIR,
      this->m_plugin_dir.value().c_str());

  if (this->m_default_auth.has_value())
    mysql_options(connection, MYSQL_DEFAULT_AUTH,
      this->m_default_auth.value().c_str());

  mysql_options(connection, MYSQL_OPT_CONNECT_ATTR_RESET, 0);
  mysql_options4(connection, MYSQL_OPT_CONNECT_ATTR_ADD,
                  "program_name", this->m_program->get_name().c_str());

  if (!mysql_real_connect(connection,
    this->get_null_or_string(this->m_host),
    this->get_null_or_string(this->m_user),
    this->get_null_or_string(this->m_password), NULL,
    this->m_mysql_port,
    this->get_null_or_string(this->m_mysql_unix_port), 0))
  {
    this->db_error(connection, "while connecting to the MySQL server");
    mysql_close(connection);
    return NULL;
  }

  return connection;
}

CHARSET_INFO* Mysql_connection_options::get_current_charset() const
{
  return m_default_charset.has_value()
    ? get_charset_by_csname(
      m_default_charset.value().c_str(), MY_CS_PRIMARY, MYF(MY_WME))
    : NULL;
}

void Mysql_connection_options::set_current_charset(CHARSET_INFO* charset)
{
  m_default_charset= string(charset->csname);
}

const char* Mysql_connection_options::get_null_or_string(
  Nullable<string>& maybe_string)
{
  if (maybe_string.has_value())
  {
    return maybe_string.value().c_str();
  }
  else
  {
    return NULL;
  }
}

#ifdef _WIN32
void Mysql_connection_options::pipe_protocol_callback(
  char* not_used MY_ATTRIBUTE((unused)))
{
  this->m_protocol= MYSQL_PROTOCOL_PIPE;
}
#endif

void Mysql_connection_options::protocol_callback(
  char* not_used MY_ATTRIBUTE((unused)))
{
  this->m_protocol=
    find_type_or_exit(this->m_protocol_string.value().c_str(),
    &sql_protocol_typelib, "protocol");
}

void Mysql_connection_options::secure_auth_callback(
  char* not_used MY_ATTRIBUTE((unused)))
{
  /* --secure-auth is a zombie option. */
  if (!this->m_secure_auth)
  {
    my_printf_error(0, "--skip-secure-auth is not supported.\n", MYF(0));
    exit(1);
  }
  else
    CLIENT_WARN_DEPRECATED_NO_REPLACEMENT("--secure-auth");
}


void Mysql_connection_options::db_error(
  MYSQL* connection, const char* when)
{
  my_printf_error(0,"Got error: %d: %s %s", MYF(0),
    mysql_errno(connection), mysql_error(connection), when);
  this->m_program->error(Mysql::Tools::Base::Message_data(
    EXIT_CANNOT_CONNECT_TO_SERVICE, "", Message_type_error));
}
