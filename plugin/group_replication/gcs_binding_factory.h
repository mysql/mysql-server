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

#ifndef GCS_BINDING_FACTORY_INCLUDED
#define	GCS_BINDING_FACTORY_INCLUDED

#include "gcs_interface.h"

//A list of all available bindings
typedef enum en_available_bindings
{
  COROSYNC
} plugin_gcs_bindings;

/*
 @class Gcs_binding_factory

  This class implements a generic way to retrieve binding implementations in the
  the plugin
 */
class Gcs_binding_factory
{
public:
  Gcs_binding_factory();
  virtual ~Gcs_binding_factory();

/**
  Retrieve a specific Gcs_interface (Binding) implementation

  @param to_retrieve the binding that one wants to retrieve

  @return a reference to a Gcs_interface object
    @retval NULL  In case of a non-existing binding or error
*/
  static Gcs_interface* get_gcs_implementation
                              (plugin_gcs_bindings binding_implementation_type);

/**
  Commands a given binding to execute its internal cleanup procedures
  It also deletes any internal references that it might contain to the
  provided implementation.

  @param binding_implementation_type Binding type to cleanup
*/
  static void cleanup_gcs_implementation
                              (plugin_gcs_bindings binding_implementation_type);
};

#endif	/* GCS_BINDING_FACTORY_INCLUDED */

