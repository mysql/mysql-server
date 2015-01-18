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

#ifndef GCS_CONTROL_DATA_EXCHANGE_EVENT_LISTENER_INCLUDED
#define GCS_CONTROL_DATA_EXCHANGE_EVENT_LISTENER_INCLUDED

#include "gcs_types.h"

#include <vector>

using std::vector;

/**
  @class Gcs_control_data_exchange_event_listener

  This interface is to be implemented by those who wish to receive the
  data that the Control Interface exchanges at group joining time
 */
class Gcs_control_data_exchange_event_listener
{
public:
  /**
   This method is called when the data exchanged is received

    @param[in] exchanged_data the exchanged data

    @return 1 in case of error
   */
  virtual int on_data(vector<uchar>* exchanged_data)= 0;

  virtual ~Gcs_control_data_exchange_event_listener(){}
};

#endif // gcs_control_data_exchange_event_listener_included
