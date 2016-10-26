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

#ifndef GCS_GROUP_MANAGEMENT_INTERFACE_INCLUDED
#define GCS_GROUP_MANAGEMENT_INTERFACE_INCLUDED

#include "gcs_types.h"

class Gcs_group_management_interface
{
public:
  /**
    Method that allows sending of a new group configuration. This is
    to be used when the group is blocked due to a loss of majority and
    only on one node from the new group.

    Implementations in each binding might have there specificities but some
    rules must be followed:
    - This should be a non-blocking call;
    - Typically, it shall cause a View Change, since we are forcing a new
      configuration.

    @param[in] reconfigured_group a list containing the new nodes
  */

  virtual enum_gcs_error
    modify_configuration(const Gcs_interface_parameters& reconfigured_group)= 0;


  virtual ~Gcs_group_management_interface() {};
};

#endif //GCS_GROUP_MANAGEMENT_INTERFACE_INCLUDED
