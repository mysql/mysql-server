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

#ifndef GCS_CONTROL_EVENT_LISTENER_INCLUDED
#define GCS_CONTROL_EVENT_LISTENER_INCLUDED

#include "gcs_view.h"

/**
  @class Gcs_control_event_listener

  This interface is implemented by those who wish to receive Control Interface
  notifications. Currently, it informs about View Changes, delivering the new
  installed view
 */
class Gcs_control_event_listener
{
public:
  /**
    This method is called when the view is ready to be installed

    @param[in] new_view a reference to the new view
   */
  virtual void on_view_changed(Gcs_view *new_view)= 0;

public:
  virtual ~Gcs_control_event_listener()
  {
  }
};

#endif // GCS_CONTROL_EVENT_LISTENER_INCLUDED
