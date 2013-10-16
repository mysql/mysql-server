/* Copyright (c) 2013, Oracle and/or its affiliates. All rights reserved.

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

#ifndef CERTIFICATION_HANDLER_INCLUDE
#define CERTIFICATION_HANDLER_INCLUDE

#include "../gcs_plugin_utils.h"
#include <applier_interfaces.h>

class Certification_handler : public EventHandler
{
public:
  Certification_handler();
  int handle(PipelineEvent *ev,Continuation* cont);
  int initialize();
  int terminate();
  bool is_unique();
  Handler_role get_role();
};

#endif /* CERTIFICATION_HANDLER_INCLUDE */
