/* Copyright (c) 2014, 2017, Oracle and/or its affiliates. All rights reserved.

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

#include "mysql/gcs/xplatform/my_xp_util.h"
#include "mysql/gcs/xplatform/byteorder.h"
#include <cstring>
#include <sstream>
#include <assert.h>

#include "mysql/gcs/gcs_member_identifier.h"

Gcs_member_identifier::Gcs_member_identifier(const std::string &id):
  m_member_id(id), m_uuid(Gcs_uuid::create_uuid())
{
}


Gcs_member_identifier::Gcs_member_identifier(const std::string &member_id,
                                             const Gcs_uuid &uuid):
  m_member_id(member_id), m_uuid(uuid)
{
}


const std::string& Gcs_member_identifier::get_member_id() const
{
  return m_member_id;
}


const Gcs_uuid& Gcs_member_identifier::get_member_uuid() const
{
  return m_uuid;
}


void Gcs_member_identifier::regenerate_member_uuid()
{
  m_uuid= Gcs_uuid::create_uuid();
}


bool Gcs_member_identifier::operator<(const Gcs_member_identifier &other) const
{
  return m_member_id.compare(other.m_member_id) < 0;
}


bool Gcs_member_identifier::operator==(const Gcs_member_identifier &other) const
{
  return m_member_id.compare(other.m_member_id) == 0;
}


Gcs_uuid Gcs_uuid::create_uuid()
{
  Gcs_uuid uuid;
  std::ostringstream ss;
  uint64_t value= htole64(My_xp_util::getsystime());

  ss << value;
  uuid.actual_value= ss.str();

  return uuid;
}


bool Gcs_uuid::encode(uchar **buffer, unsigned int *size) const
{
  if  (buffer == NULL || *buffer == NULL || size == NULL)
    return false;

  memcpy(*buffer, actual_value.c_str(), actual_value.size());
  *size= actual_value.size();

  return true;
}


bool Gcs_uuid::decode(const uchar *buffer, const unsigned int size)
{
  if  (buffer == NULL)
    return false;

  actual_value= std::string(
    reinterpret_cast<const char *>(buffer), static_cast<size_t>(size)
  );

  return true;
}
