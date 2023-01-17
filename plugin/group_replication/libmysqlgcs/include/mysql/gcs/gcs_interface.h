/* Copyright (c) 2014, 2023, Oracle and/or its affiliates.

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

#ifndef GCS_INTERFACE_INCLUDED
#define GCS_INTERFACE_INCLUDED

#include <string>

#include "plugin/group_replication/libmysqlgcs/include/mysql/gcs/gcs_communication_interface.h"
#include "plugin/group_replication/libmysqlgcs/include/mysql/gcs/gcs_control_interface.h"
#include "plugin/group_replication/libmysqlgcs/include/mysql/gcs/gcs_group_identifier.h"
#include "plugin/group_replication/libmysqlgcs/include/mysql/gcs/gcs_group_management_interface.h"
#include "plugin/group_replication/libmysqlgcs/include/mysql/gcs/gcs_logging.h"
#include "plugin/group_replication/libmysqlgcs/include/mysql/gcs/gcs_statistics_interface.h"

/**
 * @brief Runtime external resources that should be provided to an instance of
 * Gcs_interface
 *
 */
struct Gcs_interface_runtime_requirements {
  /**
   * @brief External network provider, if needed.
   *
   */
  std::shared_ptr<Network_provider> provider;

  /**
   * @brief Provider of Network Namespace services
   *
   */
  Network_namespace_manager *namespace_manager;
};

/**
  @class Gcs_interface

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
    Logger_interface *logger= new My_GCS_Logger_interface();
    group_if->set_logger(logger);

    Gcs_group_identifier *group_id= new Gcs_group_identifier("my_group");

    Gcs_control_interface *ctrl_if= group_if->get_control_session(group_id);

    ctrl_if->join(); // notice here that group id is not used anymore...

    // Do some operations here, retrieve other interfaces...

    ctrl_if->leave();

    group_if->finalize();
  @endcode
*/
class Gcs_interface {
 public:
  /**
    Method used by a binding implementation in order to implement any
    internal startup procedure.

    @retval GCS_OK in case of everything goes well. Any other value of
            gcs_error in case of error.
  */

  virtual enum_gcs_error initialize(
      const Gcs_interface_parameters &interface_params) = 0;

  /**
    Method used to report if the binding interface has already been
    initialized.

    @retval true if already initialized
  */

  virtual bool is_initialized() = 0;

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

  virtual enum_gcs_error configure(
      const Gcs_interface_parameters &interface_params) = 0;

  /**
    Method used by a binding implementation in order to implement any
    internal shutdown procedure.

    @retval GCS_OK in case of everything goes well. Any other value of
            gcs_error in case of error
  */

  virtual enum_gcs_error finalize() = 0;

  /**
    Method that retrieves the binding implementation of the Control Session
    interface.

    @param[in] group_identifier the group in which this implementation pertains

    @return A valid reference to a gcs_control_interface implementation, NULL,
            in case of error.
  */

  virtual Gcs_control_interface *get_control_session(
      const Gcs_group_identifier &group_identifier) = 0;

  /**
    Method that retrieves the binding implementation of the Communication
    Session interface.

    @param[in] group_identifier the group in which this implementation pertains

    @return A valid reference to a gcs_communication_interface implementation.
            NULL, in case of error.
  */

  virtual Gcs_communication_interface *get_communication_session(
      const Gcs_group_identifier &group_identifier) = 0;

  /**
    Method that retrieves the binding implementation of the Statistics
    interface.

    @param[in] group_identifier the group in which this implementation pertains

    @return A valid reference to a gcs_statistics_interface implementation.
            NULL, in case of error.
  */

  virtual Gcs_statistics_interface *get_statistics(
      const Gcs_group_identifier &group_identifier) = 0;

  /**
    Method that retrieves the binding implementation of the Group
    Management Session interface.

    @param[in] group_identifier the group in which this implementation pertains

    @return A valid reference to a Gcs_group_management_interface
    implementation, NULL, in case of error.
  */
  virtual Gcs_group_management_interface *get_management_session(
      const Gcs_group_identifier &group_identifier) = 0;

  /**
    Method that retrieves the binding implementation of the Group
    Management Session interface.

    @param[in] logger the logger implementation for GCS to use

    @retval GCS_OK in case of everything goes well. Any other value of
            gcs_error in case of error
  */
  virtual enum_gcs_error set_logger(Logger_interface *logger) = 0;

  /**
   * @brief Set the up runtime resources
   *
   * @param reqs a Gcs_interface_runtime_requirements filled with all the
   * requirements
   */
  virtual enum_gcs_error setup_runtime_resources(
      Gcs_interface_runtime_requirements &reqs) = 0;

  /**
   * @brief Does the necessary cleanup for runtime resources
   *
   * @param reqs a Gcs_interface_runtime_requirements filled with all the
   * requirements and their references
   */
  virtual enum_gcs_error cleanup_runtime_resources(
      Gcs_interface_runtime_requirements &reqs) = 0;

  virtual ~Gcs_interface() = default;
};

/**
  Enum that lists all implementations of Gcs_interface available to be
  returned
*/
enum enum_available_interfaces {
  /* XCom binding implementation */
  XCOM,
  NONE
};

/**
  @class Gcs_interface_factory

  @brief This class shall be used by an API user as an aggregator utility to
  retrieve implementations of Gcs_interface.
*/
class Gcs_interface_factory {
 public:
  /**
    Static method that allows retrieval of an instantiated implementation
    of a binding implementation.

    @param[in] binding an enum value of the binding implementation to retrieve.

    @return An instantiated object of a binding implementation.NULL in case of
            error.
  */

  static Gcs_interface *get_interface_implementation(
      enum_available_interfaces binding);

  /**
    Static method that allows retrieval of an instantiated implementation
    of a binding implementation.

    @param[in] binding a string matching the enum available_interfaces value of
               the binding implementation to retrieve.

    @return An instantiated object of a binding implementation. NULL in case of
            error.
  */

  static Gcs_interface *get_interface_implementation(
      const std::string &binding);

  /**
    Static method that allows the cleanup of the Gcs_interface singleton
    instance according to the binding parameter.

    @param[in] binding an enum value of the binding implementation to retrieve.
  */

  static void cleanup(enum_available_interfaces binding);

  /**
    Static method that allows the cleanup of the Gcs_interface singleton
    instance according to the binding parameter.

    @param[in] binding a string matching the enum available_interfaces value of
                       the binding implementation to retrieve.
  */

  static void cleanup(const std::string &binding);

  /**
    Static method that cleans up thread-local communication resources in the
    Gcs_interface singleton instance according to the binding parameter.
    This is required by the XCom backend when SSL is provided by OpenSSL.

    @param[in] binding an enum value of the binding implementation to retrieve.
  */

  static void cleanup_thread_communication_resources(
      enum_available_interfaces binding);

  /**
    Static method that cleans up thread-local communication resources in the
    Gcs_interface singleton instance according to the binding parameter.
    This is required by the XCom backend when SSL is provided by OpenSSL.

    @param[in] binding a string matching the enum available_interfaces value of
                       the binding implementation to retrieve.
  */

  static void cleanup_thread_communication_resources(
      const std::string &binding);

 private:
  static enum_available_interfaces from_string(const std::string &binding);
};

#endif  // gcs_interface_INCLUDED
