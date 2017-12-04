/* Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA */

#include <my_global.h>
#include "log_client.h"

Ldap_logger::Ldap_logger()
{
  m_log_level= LDAP_LOG_LEVEL_NONE;
  m_log_writer= NULL;
  m_log_writer= new Ldap_log_writer_error();
}


Ldap_logger::~Ldap_logger()
{
  if (m_log_writer)
  {
    delete m_log_writer;
  }
}


void Ldap_logger::set_log_level(ldap_log_level level)
{
  m_log_level= level;
}

void Ldap_log_writer_error::write(std::string data)
{
  std::cerr << data << "\n";
  std::cerr.flush();
}

/**
  This class writes error into default error streams.
  We needed this constructor because of template class usage.
*/
Ldap_log_writer_error::Ldap_log_writer_error()
{
}

/**
*/
Ldap_log_writer_error::~Ldap_log_writer_error()
{
}
