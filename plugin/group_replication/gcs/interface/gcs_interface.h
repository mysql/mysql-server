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

#ifndef GCS_INTERFACE_INCLUDED
#define GCS_INTERFACE_INCLUDED

#include "gcs_communication_interface.h"
#include "gcs_control_interface.h"
#include "gcs_group_identifier.h"
#include "gcs_statistics_interface.h"

/**
 @interface Gcs_interface

 This interface must be implemented by all specific binding implementations
 as its entry point.

 It provides two main functionalities:
 - Binding startup and finish
 - Allow access to the control, communication and statistics interface
 */
class Gcs_interface
{
public:

  /**
    Method used by a binding implementation in order to implement any
    internal startup procedure

    @return true in case of error
   */
  virtual bool initialize()= 0;

  /**
    Method used to report if the binding interface has already been initialized

    @return true if already initialized
   */
  virtual bool is_initialized()= 0;

  /**
   Method used by a binding implementation in order to implement any
   internal shutdown procedure

   @return true in case of error
   */
  virtual bool finalize()= 0;

  /**
    Method that retrieves the binding implementation of the Control Session
    interface

    @param[in] group_identifier the group in which this implementation pertains
    @return
      @retval A valid reference to a gcs_control_interface implementation
      @retval NULL, in case of error.
   */
  virtual Gcs_control_interface*
                  get_control_session(Gcs_group_identifier group_identifier)= 0;

  /**
    Method that retrieves the binding implementation of the Communication
    Session interface

    @param[in] group_identifier the group in which this implementation pertains
    @return
      @retval A valid reference to a gcs_communication_interface implementation.
      @retval NULL, in case of error.
   */
  virtual Gcs_communication_interface*
            get_communication_session(Gcs_group_identifier group_identifier)= 0;

  /**
    Method that retrieves the binding implementation of the Statistics
    interface

    @param[in] group_identifier the group in which this implementation pertains
    @return
      @retval A valid reference to a gcs_statistics_interface implementation.
      @retval NULL, in case of error.
   */
  virtual Gcs_statistics_interface*
                       get_statistics(Gcs_group_identifier group_identifier)= 0;

  virtual ~Gcs_interface()
  {
  }
};

#endif // gcs_interface_INCLUDED
