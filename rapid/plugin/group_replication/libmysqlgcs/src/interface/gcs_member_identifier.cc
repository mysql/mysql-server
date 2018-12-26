/* Copyright (c) 2014, 2018, Oracle and/or its affiliates. All rights reserved.

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

#include <xplatform/my_xp_util.h>
#include <xplatform/byteorder.h>
#include <cstring>
#include <assert.h>
#include <sstream>

#include "gcs_member_identifier.h"

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


Gcs_uuid::Gcs_uuid() : actual_value(do_create_uuid())
{
}


const std::string Gcs_uuid::do_create_uuid()
{
  /* Although it is possible to have collisions if different nodes create
     the same UUID, this is not a problem because the UUID is only used to
     distinguish two situations:

       . whether someone is trying to remove a newer node's incarnation.

       . whether a new node's incarnation is trying to rejoin a group when
         there are still references to its old incarnation.

     So although there might be collisions, this is not a problem because
     the actual node's identification is the combination of address and
     UUID.

     Note that, whatever the UUID is, we have to guarantee that successive
     node's incarnations don't have the same UUID.

     Our current solution uses a simple timestamp which is safe because it
     is very unlikely that the same node will be able to join, fail/leave
     and rejoin again and will keep the same uuid.

     In the future, we can start generating real UUIDs if we need them for
     any reason. The server already has the code to do it, so we could make
     this an option and pass the information to GCS.
  */
  uint64_t value= My_xp_util::getsystime();
  std::ostringstream ss;

  ss << value;
  return ss.str();
}


Gcs_uuid Gcs_uuid::create_uuid()
{
  Gcs_uuid uuid;
  uuid.actual_value= do_create_uuid();
  return uuid;
}


bool Gcs_uuid::encode(uchar **buffer, unsigned int *size) const
{
  if  (buffer == NULL || *buffer == NULL || size == NULL)
  {
/* purecov: begin tested */
    return false;
/* purecov: end */
  }

  /*
    Note the value's size will not exceed the unsigned int.
  */
  memcpy(*buffer, actual_value.c_str(), actual_value.size());
  *size= static_cast<unsigned int>(actual_value.size());

  return true;
}


bool Gcs_uuid::decode(const uchar *buffer, const unsigned int size)
{
  if  (buffer == NULL)
  {
/* purecov: begin tested */
    return false;
/* purecov: end */
  }

  actual_value= std::string(
    reinterpret_cast<const char *>(buffer), static_cast<size_t>(size)
  );

  return true;
}


size_t Gcs_uuid::size() const
{
  return actual_value.size();
}
