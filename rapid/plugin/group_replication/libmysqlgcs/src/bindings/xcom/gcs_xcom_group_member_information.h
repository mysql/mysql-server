/* Copyright (c) 2015, 2016, Oracle and/or its affiliates. All rights reserved.

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

#ifndef GCS_XCOM_GROUP_MEMBER_INFORMATION_H
#define GCS_XCOM_GROUP_MEMBER_INFORMATION_H

#include "xcom_common.h"

#include <string>

class Gcs_xcom_group_member_information
{
public:
  explicit Gcs_xcom_group_member_information(std::string member_address);

  virtual ~Gcs_xcom_group_member_information();

  std::string &get_member_address();

  std::string &get_member_ip();

  xcom_port get_member_port();

  std::string *get_member_representation() const;

private:
  std::string  m_member_address;
  std::string  m_member_ip;
  xcom_port m_member_port;
};

#endif  /* GCS_XCOM_GROUP_MEMBER_INFORMATION_H */
