/*
   Copyright (c) 2015, 2016, Oracle and/or its affiliates. All rights reserved.

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

#include "xpl_system_variables.h"


namespace xpl
{

int          Plugin_system_variables::max_connections;
unsigned int Plugin_system_variables::xport;
unsigned int Plugin_system_variables::min_worker_threads;
unsigned int Plugin_system_variables::idle_worker_thread_timeout;
unsigned int Plugin_system_variables::max_allowed_packet;
unsigned int Plugin_system_variables::connect_timeout;

Ssl_config Plugin_system_variables::ssl_config;

std::vector<Plugin_system_variables::Value_changed_callback> Plugin_system_variables::m_callbacks;

void Plugin_system_variables::clean_callbacks()
{
  m_callbacks.clear();
}

void Plugin_system_variables::registry_callback(Value_changed_callback callback)
{
  m_callbacks.push_back(callback);
}

Ssl_config::Ssl_config()
: ssl_key(NULL), ssl_ca(NULL), ssl_capath(NULL), ssl_cert(NULL), ssl_cipher(NULL), ssl_crl(NULL), ssl_crlpath(NULL), m_null_char(0)
{
}

bool Ssl_config::is_configured() const
{
  return has_value(ssl_key) ||
         has_value(ssl_ca) ||
         has_value(ssl_capath) ||
         has_value(ssl_cert) ||
         has_value(ssl_cipher) ||
         has_value(ssl_crl) ||
         has_value(ssl_crlpath);
}

/*void Ssl_config::set_not_null_value()
{
  if (!has_value(ssl_key))
    ssl_key = &m_null_char;
  if (!has_value(ssl_ca))
    ssl_ca = &m_null_char;
  if (!has_value(ssl_capath))
    ssl_capath = &m_null_char;
  if (!has_value(ssl_cert))
    ssl_cert = &m_null_char;
  if (!has_value(ssl_cipher))
    ssl_cipher = &m_null_char;
  if (!has_value(ssl_crl))
    ssl_crl = &m_null_char;
  if (!has_value(ssl_crlpath))
    ssl_crlpath = &m_null_char;
}*/

bool Ssl_config::has_value(const char *ptr) const
{
  return ptr && *ptr;
}

} // namespace xpl
