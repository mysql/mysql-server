/* Copyright (c) 2016, 2018, Oracle and/or its affiliates. All rights reserved.

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

#ifndef GCS_XCOM_GROUP_MANAGEMENT_INCLUDED
#define GCS_XCOM_GROUP_MANAGEMENT_INCLUDED

#include "gcs_group_management_interface.h" // Base class: Gcs_group_management_interface
#include "gcs_xcom_utils.h"
#include "gcs_xcom_state_exchange.h"
#include "xplatform/my_xp_mutex.h"

class Gcs_xcom_group_management : public Gcs_group_management_interface
{
public:
  explicit Gcs_xcom_group_management(
    Gcs_xcom_proxy *xcom_proxy,
    Gcs_xcom_view_change_control_interface *view_control,
    const Gcs_group_identifier& group_identifier);
  virtual ~Gcs_xcom_group_management();

  enum_gcs_error
       modify_configuration(const Gcs_interface_parameters& reconfigured_group);
  void save_xcom_nodes(const Gcs_xcom_nodes *xcom_nodes);

private:
  Gcs_xcom_proxy *m_xcom_proxy;
  Gcs_xcom_view_change_control_interface *m_view_control;
  Gcs_group_identifier* m_gid;
  unsigned int m_gid_hash;
  Gcs_xcom_nodes m_xcom_nodes;
  My_xp_mutex_impl m_xcom_nodes_mutex;

  /*
    Disabling the copy constructor and assignment operator.
  */
  Gcs_xcom_group_management(Gcs_xcom_group_management const&);
  Gcs_xcom_group_management& operator=(Gcs_xcom_group_management const&);
};
#endif // GCS_XCOM_GROUP_MANAGEMENT_INCLUDED
