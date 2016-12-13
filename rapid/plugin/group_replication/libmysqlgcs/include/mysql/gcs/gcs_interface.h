/* Copyright (c) 2014, 2016, Oracle and/or its affiliates. All rights reserved.

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

#ifndef GCS_INTERFACE_INCLUDED
#define GCS_INTERFACE_INCLUDED

#include "gcs_logging.h"
#include "gcs_communication_interface.h"
#include "gcs_control_interface.h"
#include "gcs_group_identifier.h"
#include "gcs_statistics_interface.h"
#include "gcs_group_management_interface.h"


/**
  @interface Gcs_interface

  This interface must be implemented by all specific binding implementations
  as its entry point.

  It should afterwards be distributed via a Factory, in order to allow
  its transparent instantiation.

  All of the interfaces are group-oriented, meaning that all methods that
  allow the retrieval of sub-interfaces (control, communication,
  statistics) are oriented to serve all operations to a single group.

  It provides two main functionalities:
  - Binding startup and finish;
  - Allow access to the control, communication and statistics interface.

  A typical usage of this interface shall be:

  @code{.cpp}
    Gcs_interface *group_if= new My_GCS_Gcs_interface();

    Gcs_interface_parameters params;
    params.add_parameter("name1", "value2");
    if (!group_if->is_initialized())
    {
      group_if->initialize(params);
    }

    // Inject a logger if wanted
    Ext_logger_interface *logger= new My_GCS_Ext_logger_interface();
    group_if->set_logger(logger);

    Gcs_group_identifier *group_id= new Gcs_group_identifier("my_group");

    Gcs_control_interface *ctrl_if= group_if->get_control_session(group_id);

    ctrl_if->join(); // notice here that group id is not used anymore...

    // Do some operations here, retrieve other interfaces...

    ctrl_if->leave();

    group_if->finalize();
  @endcode
*/
class Gcs_interface
{
public:

  /**
    Method used by a binding implementation in order to implement any
    internal startup procedure.

    @retval GCS_OK in case of everything goes well. Any other value of
            gcs_error in case of error.
  */

  virtual enum_gcs_error
  initialize(const Gcs_interface_parameters &interface_params)= 0;


  /**
    Method used to report if the binding interface has already been
    initialized.

    @retval true if already initialized
  */

  virtual bool is_initialized()= 0;


  /**
    Method used by a binding implementation in order to implement any type of
    necessary dynamic reconfiguration.

    An example of this could be an underlying GCS that needs to adjust itself
    to changes in a group. Note, however, that the method must be only used
    when the system is not running in order to avoid possible concurrency
    issues. Using cached information by the caller, after this member function
    has been called, results in undefined behavior.

    @retval GCS_OK in case of everything goes well. Any other value of
            gcs_error in case of error.
  */

  virtual enum_gcs_error
  configure(const Gcs_interface_parameters &interface_params)= 0;


  /**
    Method used by a binding implementation in order to implement any
    internal shutdown procedure.

    @retval GCS_OK in case of everything goes well. Any other value of
            gcs_error in case of error
  */

  virtual enum_gcs_error finalize()= 0;


  /**
    Method that retrieves the binding implementation of the Control Session
    interface.

    @param[in] group_identifier the group in which this implementation pertains

    @return A valid reference to a gcs_control_interface implementation, NULL,
            in case of error.
  */

  virtual Gcs_control_interface *
  get_control_session(const Gcs_group_identifier &group_identifier)= 0;


  /**
    Method that retrieves the binding implementation of the Communication
    Session interface.

    @param[in] group_identifier the group in which this implementation pertains

    @return A valid reference to a gcs_communication_interface implementation.
            NULL, in case of error.
  */

  virtual Gcs_communication_interface *
  get_communication_session(const Gcs_group_identifier &group_identifier)= 0;


  /**
    Method that retrieves the binding implementation of the Statistics
    interface.

    @param[in] group_identifier the group in which this implementation pertains

    @return A valid reference to a gcs_statistics_interface implementation.
            NULL, in case of error.
  */

  virtual Gcs_statistics_interface *
  get_statistics(const Gcs_group_identifier &group_identifier)= 0;


  /**
    Method that retrieves the binding implementation of the Group
    Management Session interface.

    @param[in] group_identifier the group in which this implementation pertains

    @return A valid reference to a Gcs_group_management_interface implementation,
            NULL, in case of error.
  */
  virtual Gcs_group_management_interface *
        get_management_session(const Gcs_group_identifier &group_identifier)= 0;


  /**
    Method that retrieves the binding implementation of the Group
    Management Session interface.

    @param[in] logger the logger implementation for GCS to use

    @retval GCS_OK in case of everything goes well. Any other value of
            gcs_error in case of error
  */
  virtual enum_gcs_error set_logger(Ext_logger_interface *logger)= 0;


  virtual ~Gcs_interface() {}
};


/**
  @enum available_interfaces

  @brief Enum that lists all implementations of Gcs_interface available to be
         returned
*/
enum enum_available_interfaces
{
  /* XCom binding implementation */
  XCOM,
  NONE
};


/**
  @class Gcs_interface_factory

  @brief This class shall be used by an API user as an aggregator utility to
  retrieve implementations of Gcs_interface.
*/
class Gcs_interface_factory
{
public:
  /**
    Static method that allows retrieval of an instantiated implementation
    of a binding implementation.

    @param[in] binding an enum value of the binding implementation to retrieve.

    @return An instantiated object of a binding implementation.NULL in case of
            error.
  */

  static Gcs_interface*
  get_interface_implementation(enum_available_interfaces binding);


  /**
    Static method that allows retrieval of an instantiated implementation
    of a binding implementation.

    @param[in] binding a string matching the enum available_interfaces value of
               the binding implementation to retrieve.

    @return An instantiated object of a binding implementation. NULL in case of
            error.
  */

  static Gcs_interface *
  get_interface_implementation(const std::string &binding);


  /**
    Static method that allows the cleanup of the Gcs_interface singleton
    instance according to the binding parameter.

    @param[in] binding an enum value of the binding implementation to retrieve.

    @return An instantiated object of a binding implementation.NULL in case of
            error
  */

  static
  void cleanup(enum_available_interfaces binding);


  /**
    Static method that allows the cleanup of the Gcs_interface singleton
    instance according to the binding parameter.

    @param[in] binding a string matching the enum available_interfaces value of
                       the binding implementation to retrieve.
  */

  static
  void cleanup(const std::string& binding);


private:
  static enum_available_interfaces from_string(const std::string &binding);
};

#endif // gcs_interface_INCLUDED
