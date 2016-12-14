/* Copyright (c) 2016, Oracle and/or its affiliates. All rights reserved.

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

#ifndef GCS_XCOM_GROUP_MANAGEMENT_INCLUDED
#define GCS_XCOM_GROUP_MANAGEMENT_INCLUDED

#include "gcs_group_management_interface.h" // Base class: Gcs_group_management_interface
#include "gcs_xcom_utils.h"

class Gcs_xcom_group_management : public Gcs_group_management_interface
{
public:
  explicit Gcs_xcom_group_management(
    Gcs_xcom_proxy *xcom_proxy,
    const Gcs_group_identifier& group_identifier);
  virtual ~Gcs_xcom_group_management();

  enum_gcs_error
       modify_configuration(const Gcs_interface_parameters& reconfigured_group);

private:
  Gcs_xcom_proxy *m_xcom_proxy;
  Gcs_group_identifier* m_gid;
  unsigned int m_gid_hash;

  /*
    Disabling the copy constructor and assignment operator.
  */
  Gcs_xcom_group_management(Gcs_xcom_group_management const&);
  Gcs_xcom_group_management& operator=(Gcs_xcom_group_management const&);
};
#endif // GCS_XCOM_GROUP_MANAGEMENT_INCLUDED
