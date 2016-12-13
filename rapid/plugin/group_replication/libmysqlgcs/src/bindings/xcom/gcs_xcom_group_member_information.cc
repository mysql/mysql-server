/* Copyright (c) 2015, 2016, Oracle and/or its affiliates. All rights reserved.

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

#include "gcs_xcom_group_member_information.h"
#include "gcs_xcom_utils.h"

#include <cstdlib>

Gcs_xcom_group_member_information::
Gcs_xcom_group_member_information(std::string member_address)
  :m_member_address(member_address),
  m_member_ip(),
  m_member_port(0)
{
  std::size_t idx= member_address.find(":");

  if (idx != std::string::npos)
  {
    m_member_ip.append(m_member_address, 0, idx);
    std::string sport;
    sport.append(m_member_address, idx + 1, m_member_address.size() - idx);
    m_member_port= static_cast<xcom_port>(strtoul(sport.c_str(), NULL, 0));
  }
}


std::string &Gcs_xcom_group_member_information::get_member_address()
{
  return m_member_address;
}


std::string &Gcs_xcom_group_member_information::get_member_ip()
{
  return m_member_ip;
}


xcom_port Gcs_xcom_group_member_information::get_member_port()
{
  return m_member_port;
}


std::string *
Gcs_xcom_group_member_information::get_member_representation() const
{
  return new std::string(m_member_address);
}


Gcs_xcom_group_member_information::~Gcs_xcom_group_member_information() {}
