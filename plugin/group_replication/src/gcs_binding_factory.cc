/* Copyright (c) 2014, 2015, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */

#include "gcs_binding_factory.h"

#include "gcs_corosync_interface.h"

Gcs_binding_factory::Gcs_binding_factory()
{
}

Gcs_binding_factory::~Gcs_binding_factory()
{
}

Gcs_interface*
Gcs_binding_factory::get_gcs_implementation
                               (plugin_gcs_bindings binding_implementation_type)
{
  Gcs_interface* gcs_binding= NULL;

  switch(binding_implementation_type)
  {
  case COROSYNC:
    gcs_binding= Gcs_corosync_interface::get_interface();
    break;
  default:
    break;
  }

  return gcs_binding;
}

void
Gcs_binding_factory::
cleanup_gcs_implementation(plugin_gcs_bindings binding_implementation_type)
{
  switch(binding_implementation_type)
  {
  case COROSYNC:
    Gcs_corosync_interface::cleanup();
    break;
  default:
    break;
  }
}
