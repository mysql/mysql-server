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

#ifndef EVENT_CATALOGER_INCLUDE
#define EVENT_CATALOGER_INCLUDE

#include "../gcs_applier.h"
#include <applier_interfaces.h>

class Event_cataloger : public EventHandler
{
public:
  Event_cataloger();
  int handle(PipelineEvent *ev,Continuation* cont);
  int initialize();
  int terminate();
  bool is_unique();
  Handler_role get_role();
};

#endif /* EVENT_CATALOGER_INCLUDE */
