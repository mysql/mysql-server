/* Copyright (c) 2015, 2019, Oracle and/or its affiliates. All rights reserved.

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

#ifndef GCS_XCOM_PROXY_INCLUDED
#define GCS_XCOM_PROXY_INCLUDED

#include <string>
#include <unordered_set>  // std::unordered_set
#include "plugin/group_replication/libmysqlgcs/include/mysql/gcs/gcs_types.h"
#include "plugin/group_replication/libmysqlgcs/include/mysql/gcs/xplatform/my_xp_cond.h"
#include "plugin/group_replication/libmysqlgcs/include/mysql/gcs/xplatform/my_xp_mutex.h"
#include "plugin/group_replication/libmysqlgcs/include/mysql/gcs/xplatform/my_xp_util.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/gcs_internal_message.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/gcs_xcom_group_member_information.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/gcs_xcom_input_queue.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/node_connection.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/site_struct.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/xcom_common.h"
#include "plugin/group_replication/libmysqlgcs/xdr_gen/xcom_vp.h"

#define XCOM_COMM_STATUS_UNDEFINED -1

/**
  @class gcs_xcom_control_proxy

  This class is an abstraction layer between xcom and the actual
  implementation. The purpose of this is to allow Gcs_xcom_control_interface
  to be unit testable by creating mock classes on top of it.
*/
class Gcs_xcom_proxy {
 public:
  explicit Gcs_xcom_proxy() {}

  /**
    The destructor.
  */
  virtual ~Gcs_xcom_proxy() {}

  /**
    This is an utility member function that is used to call into XCom for
    creating list with node's addresses and their associated UUIDs. Note
    that callers must provide the UUID.

    @param n The number of elements in the list
    @param names The names to be put on the list
    @param uuids The UUIDs to be put on the list
    @return a pointer to the list containing all the elements needed. The
    caller is responsible to reclaim memory once he is done with this data
    @c delete_node_address
  */

  virtual node_address *new_node_address_uuid(unsigned int n, char *names[],
                                              blob uuids[]) = 0;

  /**
    This function is responsible to delete the list of nodes that had been
    previously created by @c new_node_address.

    @param n the length of the list
    @param na the list to delete

    @return false on success, true otherwise. In case of an error, memory may
    not have been completely freed.
  */

  virtual void delete_node_address(unsigned int n, node_address *na) = 0;

  /**
    This member function is responsible to call into XCom consensus and add a
    node to the group. The caller is responsible for ensuring that the session
    has been opened before @c open_session and also that the node is not yet
    in the configuration.

    The callee must have opened an XCom connection before calling this
    function. @c xcom_client_open_connection.

    @param fd the file descriptor to the XCom connection established earlier
    @param nl The node list containing the list of peers to add
    @param group_id the identifier of the group to which the nodes should
           be added
    @returns true (false) on success (failure). Success means that XCom will
             process our request, failure means it wont. There could be errors
             later in the process of adding a node. Since this is basically an
             asynchronous function, one needs to wait for the actual view change
             to validate that the node was added to the configuration.
  */

  virtual bool xcom_client_add_node(connection_descriptor *fd, node_list *nl,
                                    uint32_t group_id) = 0;

  /**
    This member function is responsible for triggering the removal of a node
    from the XCom configuration. This function is asynchronous, so you need to
    wait for the view change to actually validate that the removal was
    successful.

    The caller is responsible for making sure that the server to be removed is
    in the group.

    This function MUST be called after opening the local XCom session
    @c xcom_input_connect.

    @param nl The list of nodes to remove from the group
    @param group_id The identifier of the group from which the nodes will
           be removed
    @returns true (false) on success (failure). Success means that XCom will
             process our request, failure means it wont. There could be errors
             later in the process of removing a node. Since this is basically an
             asynchronous function, one needs to wait for the actual view change
             to validate that the nodes were removed from the configuration.
  */

  virtual bool xcom_client_remove_node(node_list *nl, uint32_t group_id) = 0;

  /**
    This member function is responsible for triggering the removal of a node
    from the XCom configuration. This function is asynchronous, so you need to
    wait for the view change to actually validate that the removal was
    successful.

    The caller is responsible for making sure that the server to be removed is
    in the group.

    The callee must have opened an XCom connection before calling this
    function. @c xcom_client_open_connection.

    @param fd the file descriptor to the XCom connection established earlier
    @param nl The list of nodes to remove from the group
    @param group_id The identifier of the group from which the nodes will
           be removed
    @returns true (false) on success (failure). Success means that XCom will
             process our request, failure means it wont. There could be errors
             later in the process of removing a node. Since this is basically an
             asynchronous function, one needs to wait for the actual view change
             to validate that the nodes were removed from the configuration.
  */

  virtual bool xcom_client_remove_node(connection_descriptor *fd, node_list *nl,
                                       uint32_t group_id) = 0;

  /**
    This member function is responsible for retrieving the event horizon of the
    XCom configuration.

    This function REQUIRES a prior call to @c xcom_open_handlers to establish a
    connection to XCom.

    @param[in] group_id The identifier of the group from which the event horizon
               will be retrieved
    @param[out] event_horizon A reference to where the group's event horizon
                value will be written to
    @retval true if successful and @c event_horizon was written to
    @retval false otherwise

 */
  virtual bool xcom_client_get_event_horizon(
      uint32_t group_id, xcom_event_horizon &event_horizon) = 0;

  /**
    This member function is responsible for triggering the reconfiguration of
    the event horizon of the XCom configuration. This function is asynchronous,
    so you need to poll @c xcom_get_event_horizon to actually validate that the
    reconfiguration was successful.

    This function REQUIRES a prior call to @c xcom_open_handlers to establish a
    connection to XCom.

    @param event_horizon The desired event horizon value
    @param group_id The identifier of the group from which the nodes will
           be removed
    @returns true (false) on success (failure). Success means that XCom will
             process our request, failure means it wont. There could be errors
             later in the process of setting the event horizon. Since this is
             basically an asynchronous function, one needs to busy-wait on
             @c xcom_client_get_event_horizon to validate that the event horizon
             was modified.
  */
  virtual bool xcom_client_set_event_horizon(
      uint32_t group_id, xcom_event_horizon event_horizon) = 0;

  /**
   This member function is responsible for retrieving the application payloads
   decided in the synodes in @c synodes, from the XCom instance connected to
   via @c fd.

   @param[in] fd The file descriptor to the XCom connection established
   earlier
   @param[in] group_id The identifier of the group from which the payloads
   will be retrieved
   @param[in] synodes The desired synodes
   @param[out] reply Where the requested payloads will be written to
   @returns true (false) on success (failure). Success means that XCom had the
   requested payloads, which were written to @c reply.
   */
  virtual bool xcom_client_get_synode_app_data(
      connection_descriptor *fd, uint32_t group_id, synode_no_array &synodes,
      synode_app_data_array &reply) = 0;

  /**
    This member function is responsible for setting a new value for the maximum
    size of the XCom cache.

    This function is asynchronous, so the return value indicates only whether
    the request to change the value was successfully pushed to XCom or not.
    However, since the cache size is set only for the local node, and makes no
    further verifications on the value, if XCom processes the request, it will
    accept the new value. The callers of this function must validate that the
    value set is within the limits set to the maximum size of the XCom cache.

    This function REQUIRES a prior call to @c xcom_open_handlers to establish a
    connection to XCom.

    @param size The new value for the maximum size of the XCom cache.
    @returns true (false) on success (failure). Success means that XCom will
             process our request, failure means it wont.
  */
  virtual bool xcom_client_set_cache_size(uint64_t size) = 0;

  /**
    This member function is responsible for pushing data into consensus on
    XCom. The caller is responsible to making sure that there is an open XCom
    session @c xcom_input_connect and also that the server is part of the XCom
    configuration before sending data to it.

    @c data must have been allocated by one of the functions of the malloc
    family, because @c data will be passed on to XCom. XCom is implemented in
    C and will use @c free to dispose of @c data when it is finished with @c
    data.

    This function takes ownership of @c data. This means that this
    function becomes responsible for the lifetime of @c data. Since this
    function takes ownership of @c data, the caller must not interact with @c
    data after passing it to this function.

    @param size the size of the payload
    @param data the payload
    @returns true (false) on success (failure). Success means that XCom will
             process our request, failure means it wont.
  */

  virtual bool xcom_client_send_data(unsigned long long size, char *data) = 0;

  /**
    This member function initializes XCom. This function must be called before
    anything else and from within the XCom thread. It will eventually call
    the main loop inside XCom.

    @param listen_port the port that the local XCom is to be listening on.
  */

  virtual void xcom_init(xcom_port listen_port) = 0;

  /**
    This member function finishes the XCom thread. This function must be
    called when the XCOM thread was started but the node has not joined
    a group.

    @param xcom_input_open indicates whether the input channel to the XCom
                           thread has already been opened.
    @returns true (false) on success (failure). Success means that XCom will
             process our request, failure means it wont. There could be errors
             later in the process of exiting XCom. Since this is basically an
             asynchronous function, one needs to wait for the XCom thread to
             to ensure that XCom terminated.
  */

  virtual bool xcom_exit(bool xcom_input_open) = 0;

  /*
    Return the operation mode as an integer from an operation mode provided
    as a string. Note that the string must be provided in upper case letters
    and the possible values are: "DISABLED", "PREFERRED", "REQUIRED",
    "VERIFY_CA" or "VERIFY_IDENTITY".

    If a different value is provide, INVALID_SSL_MODE (-1) is returned.
  */

  virtual int xcom_get_ssl_mode(const char *mode) = 0;

  /*
    Set the operation mode which might be the following:

    . SSL_DISABLED (1): The SSL mode will be disabled and this is the default
      value.

    . SSL_PREFERRED (2): The SSL mode will be always disabled if this value is
      provided and is only allowed to keep the solution compatibility with
      MySQL server.

    . SSL_REQUIRED (4): The SSL mode will be enabled but the verifications
      described in the next modes are not performed.

    . SSL_VERIFY_CA (4) - Verify the server TLS certificate against the
    configured Certificate Authority (CA) certificates. The connection attempt
    fails if no valid matching CA certificates are found.

    . SSL_VERIFY_IDENTITY (5): Like VERIFY_CA, but additionally verify that the
      server certificate matches the host to which the connection is attempted.

    If a different value is provide, INVALID_SSL_MODE (-1) is returned.
  */

  virtual int xcom_set_ssl_mode(int mode) = 0;

  /*
    Return the operation fips mode as an integer from an operation fips mode
    provided as a string. Note that the string must be provided in upper case
    letters and the possible values are: "OFF", "ON", "STRICT",

    If a different value is provide, INVALID_SSL_MODE (-1) is returned.
  */

  virtual int xcom_get_ssl_fips_mode(const char *mode) = 0;

  /*
    Set the operation fips mode which might be the following:

    . SSL_FIPS_MODE_OFF (0): This will set openssl fips mode value to 0

    . SSL_FIPS_MODE_ON (1): This will set openssl fips mode value to 1

    . SSL_FIPS_MODE_STRICT (2): This will set openssl fips mode value to 2

    If a different value is provide, INVALID_SSL_FIPS_MODE (-1) is returned.
  */

  virtual int xcom_set_ssl_fips_mode(int mode) = 0;

  /**
    Initialize the SSL.

    @returns true (false) on success (failure)
  */

  virtual bool xcom_init_ssl() = 0;

  /*
    Destroy the SSL Configuration freeing allocated memory.
  */

  virtual void xcom_destroy_ssl() = 0;

  /**
    Return whether the SSL will be used to encrypt data or not.

    @returns true (false) if XCom will (not) use SSL
  */

  virtual bool xcom_use_ssl() = 0;

  /*
    Set the necessary SSL parameters before initialization.

    server_key_file  - Path of file that contains the server's X509 key in PEM
                       format.
    server_cert_file - Path of file that contains the server's X509 certificate
    in PEM format. client_key_file  - Path of file that contains the client's
    X509 key in PEM format. client_cert_file - Path of file that contains the
    client's X509 certificate in PEM format. ca_file          - Path of file
    that contains list of trusted SSL CAs. ca_path          - Path of directory
    that contains trusted SSL CA certificates in PEM format. crl_file         -
    Path of file that contains certificate revocation lists. crl_path         -
    Path of directory that contains certificate revocation list files. cipher
    - List of permitted ciphers to use for connection encryption. tls_version
    - Protocols permitted for secure connections.

    Note that only the server_key_file/server_cert_file and the client_key_file/
    client_cert_file are required and the rest of the pointers can be NULL.
    If the key is provided along with the certificate, either the key file or
    the other can be ommited.

    The caller can free the parameters after the SSL is started
    if this is necessary.
  */
  virtual void xcom_set_ssl_parameters(
      const char *server_key_file, const char *server_cert_file,
      const char *client_key_file, const char *client_cert_file,
      const char *ca_file, const char *ca_path, const char *crl_file,
      const char *crl_path, const char *cipher, const char *tls_version) = 0;

  virtual site_def const *find_site_def(synode_no synode) = 0;

  /**
    This member function boots XCom.

    @param nl List with a single member - the one that boots the group
    @param group_id the Group identifier to which the member belongs to
    @returns true (false) on success (failure). Success means that XCom will
             process our request, failure means it wont. There could be errors
             later in the process of booting. Since this is basically an
             asynchronous function, one needs to wait for XCom to signal it is
             ready to validate whether it booted.
  */

  virtual bool xcom_client_boot(node_list *nl, uint32_t group_id) = 0;

  /**
    This member function opens a connection to an XCom instance.

    @param addr The XCom instance address
    @param port The XCom instance port
    @return a valid file descriptor on success, -1 otherwise
  */

  virtual connection_descriptor *xcom_client_open_connection(
      std::string addr, xcom_port port) = 0;

  /**
    This member function closes the connection to an XCom instance.

    @param fd The connection file descriptor
    @returns true (false) on success (failure)
  */

  virtual bool xcom_client_close_connection(connection_descriptor *fd) = 0;

  /**
    This member waits for XCom to be initialized.
  */

  virtual enum_gcs_error xcom_wait_ready() = 0;

  /*
  This member retrieves the value of XCom initialized
  */
  virtual bool xcom_is_ready() = 0;

  /*
   This member sets the value of XCom initialized
   */
  virtual void xcom_set_ready(bool value) = 0;

  /*
   This member signals that XCom has initialized.
   */
  virtual void xcom_signal_ready() = 0;

  /**
    @brief Call this method to wait for XCom communications to be initialized.

    Call this method to wait for XCom communications to be initialized. It will
    block until XCom communications are either OK or error out. The value of
    the status (XCOM_COMMS_OK or XCOM_COMMS_ERROR) is written into the status
    out parameters.

    @param [out] status value of the XCom communication layer status.
                       It can be either XCOM_COMMS_OK or XCOM_COMMS_ERROR
   */
  virtual void xcom_wait_for_xcom_comms_status_change(int &status) = 0;

  /*
   This verifies if the communication status callback from XCom has been called
   and if the internal cached status is different from
   XCOM_COMM_STATUS_UNDEFINED
  */
  virtual bool xcom_has_comms_status_changed() = 0;

  /*
   This sets the status value of communication status callback from XCom.
   Its main purpose is to reset the internal cached status to
   XCOM_COMM_STATUS_UNDEFINED

   @param status the new status value
   */
  virtual void xcom_set_comms_status(int status) = 0;

  /*
   This modifies the internal cached status to whatever value is delivered
   by the XCom communication status callback. Then, it signals all threads
   that might be waiting on xcom_wait_for_xcom_comms_status_change.

   @param status the new status value
  */
  virtual void xcom_signal_comms_status_changed(int status) = 0;

  /**
    @brief Call this method to wait for XCom to exit.

    Call this method to wait for XCom to exit. It will block until XCom has
    exit or an error occurs.

    @return GCS_OK if success, otherwise GCS_NOK.
  */

  virtual enum_gcs_error xcom_wait_exit() = 0;

  /**
    This verifies if XCom has finished or not.
  */

  virtual bool xcom_is_exit() = 0;

  /**
    This sets whether XCom has finished or not.
  */

  virtual void xcom_set_exit(bool value) = 0;

  /**
    Clean up variables used to notify states in the XCOM's state
    machine.
  */
  virtual void xcom_set_cleanup() = 0;

  /**
    This modifies the internal cached status and signals all threads
   that might be waiting on xcom_wait_exit.
  */

  virtual void xcom_signal_exit() = 0;

  /**
    This method forces XCom to inject a new configuration in the group,
    even if it does not contain a majority of members.

    @param nl The list of nodes that will belong to this new configuration
    @param group_id The identifier of the group from which the nodes will
           belong
    @returns true (false) on success (failure). Success means that XCom will
             process our request, failure means it wont. There could be errors
             later in the process of forcing the configuration. Since this is
             basically an asynchronous function, one needs to wait for the
             actual view change to validate that the configuration was forced.
   */
  virtual bool xcom_client_force_config(node_list *nl, uint32_t group_id) = 0;

  /**
    Function used to boot a node in XCOM.

    @param node Node information.
    @param group_id_hash Hash of group identifier.
    @returns true (false) on success (failure). Success means that XCom will
             process our request, failure means it wont. There could be errors
             later in the process of booting. Since this is basically an
             asynchronous function, one needs to wait for XCom to signal it is
             ready to validate whether it booted.
  */
  virtual bool xcom_boot_node(Gcs_xcom_node_information &node,
                              uint32_t group_id_hash) = 0;

  /**
    Function to remove a set of nodes from XCOM.

    @param nodes Set of nodes.
    @param group_id_hash Hash of group identifier.
    @returns true (false) on success (failure). Success means that XCom will
             process our request, failure means it wont. There could be errors
             later in the process of removing a node. Since this is basically an
             asynchronous function, one needs to wait for the actual view change
             to validate that the nodes were removed from the configuration.
  */

  virtual bool xcom_remove_nodes(Gcs_xcom_nodes &nodes,
                                 uint32_t group_id_hash) = 0;

  /**
    Function to remove a set of nodes from XCOM.

    @param con Connection to a node that will carry on the request.
    @param nodes Set of nodes to remove.
    @param group_id_hash Hash of group identifier.
    @returns true (false) on success (failure). Success means that XCom will
             process our request, failure means it wont. There could be errors
             later in the process of removing a node. Since this is basically an
             asynchronous function, one needs to wait for the actual view change
             to validate that the nodes were removed from the configuration.
  */

  virtual bool xcom_remove_nodes(connection_descriptor &con,
                                 Gcs_xcom_nodes &nodes,
                                 uint32_t group_id_hash) = 0;

  /**
    Function to remove a node from XCOM.

    @param node Node information.
    @param group_id_hash Hash of group identifier.
    @returns true (false) on success (failure). Success means that XCom will
             process our request, failure means it wont. There could be errors
             later in the process of removing a node. Since this is basically an
             asynchronous function, one needs to wait for the actual view change
             to validate that the node was removed from the configuration.
  */

  virtual bool xcom_remove_node(const Gcs_xcom_node_information &node,
                                uint32_t group_id_hash) = 0;

  /**
    Function to add a set of nodes to XCOM.

    @param con Connection to a node that will carry on the request.
    @param nodes Set of nodes.
    @param group_id_hash Hash of group identifier.
    @returns true (false) on success (failure). Success means that XCom will
             process our request, failure means it wont. There could be errors
             later in the process of adding a node. Since this is basically an
             asynchronous function, one needs to wait for the actual view change
             to validate that the nodes were added to the configuration.
  */

  virtual bool xcom_add_nodes(connection_descriptor &con, Gcs_xcom_nodes &nodes,
                              uint32_t group_id_hash) = 0;

  /**
    Function to add a node to XCOM.

    @param con Connection to a node that will carry on the request.
    @param node Node information.
    @param group_id_hash Hash of group identifier.
    @returns true (false) on success (failure). Success means that XCom will
             process our request, failure means it wont. There could be errors
             later in the process of adding a node. Since this is basically an
             asynchronous function, one needs to wait for the actual view change
             to validate that the node was added to the configuration.
  */

  virtual bool xcom_add_node(connection_descriptor &con,
                             const Gcs_xcom_node_information &node,
                             uint32_t group_id_hash) = 0;

  /**
    Function to retrieve XCOM's minimum supported event horizon value.
  */
  virtual xcom_event_horizon xcom_get_minimum_event_horizon() = 0;

  /**
    Function to retrieve XCOM's maximum supported event horizon value.
  */
  virtual xcom_event_horizon xcom_get_maximum_event_horizon() = 0;

  /**
    Function to retrieve XCOM's event horizon.

    @param[in] group_id_hash Hash of group identifier.
    @param[out] event_horizon A reference to where the group's event horizon
                value will be written to

    @return true if successful, false otherwise
  */
  virtual bool xcom_get_event_horizon(uint32_t group_id_hash,
                                      xcom_event_horizon &event_horizon) = 0;

  /**
   Function to retrieve the application payloads decided on a set of synodes.

   @param[in] xcom_instace The XCom instance to connect to
   @param[in] group_id_hash Hash of group identifier.
   @param[in] synode_set The desired synodes
   @param[out] reply Where the requested payloads will be written to
   @returns true (false) on success (failure). Success means that XCom had the
   requested payloads, which were written to @c reply.
   */
  virtual bool xcom_get_synode_app_data(
      Gcs_xcom_node_information const &xcom_instance, uint32_t group_id_hash,
      const std::unordered_set<Gcs_xcom_synode> &synode_set,
      synode_app_data_array &reply) = 0;

  /**
    Function to reconfigure XCOM's event horizon.

    @param group_id_hash Hash of group identifier.
    @param event_horizon Desired event horizon value.

    @return true if successful, false otherwise
  */
  virtual bool xcom_set_event_horizon(uint32_t group_id_hash,
                                      xcom_event_horizon event_horizon) = 0;
  /**
    Function to reconfigure the maximum size of the XCom cache.

    @param size Cache size limit.
    @return true if the operation is successfully pushed to XCom's queue,
                 false otherwise
  */
  virtual bool xcom_set_cache_size(uint64_t size) = 0;

  /**
    Function to force the set of nodes in XCOM's configuration.

    @param nodes Set of nodes.
    @param group_id_hash Hash of group identifier.
    @returns true (false) on success (failure). Success means that XCom will
             process our request, failure means it wont. There could be errors
             later in the process of forcing the configuration. Since this is
             basically an asynchronous function, one needs to wait for the
             actual view change to validate that the configuration was forced.
  */

  virtual bool xcom_force_nodes(Gcs_xcom_nodes &nodes,
                                unsigned int group_id_hash) = 0;

  /**
    Function that retrieves the value that signals that XCom
    must be forcefully stopped.

    @return 1 if XCom needs to forcefully exit. 0 otherwise.
   */
  virtual bool get_should_exit() = 0;

  /**
    Function that sets the value that signals that XCom
    must be forcefully stopped.
   */
  virtual void set_should_exit(bool should_exit) = 0;

  /**
   * Opens the input channel to XCom.
   *
   * @retval true if successful
   * @retval false otherwise
   */
  virtual bool xcom_input_connect() = 0;

  /**
   * Attempts to send the command @c data to XCom. (Called by GCS.)
   *
   * The function takes ownership of @c data.
   *
   * @pre The input channel to XCom is open, i.e. @c xcom_input_connect
   * @param data the command to send to XCom
   * @retval true if the command was sent to XCom
   * @retval false if the command was not sent to XCom
   */
  virtual bool xcom_input_try_push(app_data_ptr data) = 0;

  /**
   * Attempts to send the command @c data to XCom, and returns a future to
   * XCom's reply. (Called by GCS.)
   *
   * The function takes ownership of @c data.
   *
   * @pre The input channel to XCom is open, i.e. @c xcom_input_connect
   * @param data the command to send to XCom
   * @returns a future pointer to a Reply object if successful, a future pointer
   * to nullptr otherwise
   */
  virtual Gcs_xcom_input_queue::future_reply xcom_input_try_push_and_get_reply(
      app_data_ptr data) = 0;

  /**
   * Attempts to retrieve incoming commands. (Called by XCom.)
   *
   * @pre The input channel to XCom is open, i.e. @c xcom_input_connect
   * @retval app_data_ptr linked list of the queued comamnds if the queue is
   *                      not empty
   * @retval nullptr if the queue is empty
   */
  virtual xcom_input_request_ptr xcom_input_try_pop() = 0;

  /**
   * Performs a test connection to the given XCom instance via TCP.
   *
   * @param host the XCom instance's host
   * @param port the XCom instance's port
   * @returns true if we were able to successfully connect, false otherwise.
   */
  virtual bool test_xcom_tcp_connection(std::string &host, xcom_port port) = 0;
};

/*
  Virtual class that contains implementation of methods used to
  map a node representation into XCOM's representation.
  This layer becomes necessary to avoid duplicating code in test
  cases.
*/
class Gcs_xcom_proxy_base : public Gcs_xcom_proxy {
 public:
  explicit Gcs_xcom_proxy_base() {}
  virtual ~Gcs_xcom_proxy_base() {}

  bool xcom_boot_node(Gcs_xcom_node_information &node, uint32_t group_id_hash);
  bool xcom_remove_nodes(Gcs_xcom_nodes &nodes, uint32_t group_id_hash);
  bool xcom_remove_nodes(connection_descriptor &con, Gcs_xcom_nodes &nodes,
                         uint32_t group_id_hash);
  bool xcom_remove_node(const Gcs_xcom_node_information &node,
                        uint32_t group_id_hash);
  bool xcom_add_nodes(connection_descriptor &con, Gcs_xcom_nodes &nodes,
                      uint32_t group_id_hash);
  bool xcom_add_node(connection_descriptor &con,
                     const Gcs_xcom_node_information &node,
                     uint32_t group_id_hash);
  xcom_event_horizon xcom_get_minimum_event_horizon();
  xcom_event_horizon xcom_get_maximum_event_horizon();
  bool xcom_get_event_horizon(uint32_t group_id_hash,
                              xcom_event_horizon &event_horizon);
  bool xcom_set_event_horizon(uint32_t group_id_hash,
                              xcom_event_horizon event_horizon);
  bool xcom_get_synode_app_data(
      Gcs_xcom_node_information const &xcom_instance, uint32_t group_id_hash,
      const std::unordered_set<Gcs_xcom_synode> &synode_set,
      synode_app_data_array &reply);
  bool xcom_set_cache_size(uint64_t size);
  bool xcom_force_nodes(Gcs_xcom_nodes &nodes, uint32_t group_id_hash);

 private:
  /* Serialize information on nodes to be sent to XCOM */
  bool serialize_nodes_information(Gcs_xcom_nodes &nodes, node_list &nl);

  /* Free information on nodes sent to XCOM */
  void free_nodes_information(node_list &nl);
  bool test_xcom_tcp_connection(std::string &host, xcom_port port);
};

/**
  @class gcs_xcom_control_proxy_impl

  Implementation of gcs_xcom_control_proxy to be used by whom
  instantiates Gcs_xcom_control_interface to be used in a real
  scenario.
*/
class Gcs_xcom_proxy_impl : public Gcs_xcom_proxy_base {
 public:
  explicit Gcs_xcom_proxy_impl();
  Gcs_xcom_proxy_impl(unsigned int wt);
  virtual ~Gcs_xcom_proxy_impl();

  node_address *new_node_address_uuid(unsigned int n, char *names[],
                                      blob uuids[]);
  void delete_node_address(unsigned int n, node_address *na);
  bool xcom_client_add_node(connection_descriptor *fd, node_list *nl,
                            uint32_t group_id);
  bool xcom_client_remove_node(node_list *nl, uint32_t group_id);
  bool xcom_client_remove_node(connection_descriptor *fd, node_list *nl,
                               uint32_t group_id);
  bool xcom_client_get_event_horizon(uint32_t group_id,
                                     xcom_event_horizon &event_horizon);
  bool xcom_client_set_event_horizon(uint32_t group_id,
                                     xcom_event_horizon event_horizon);
  bool xcom_client_get_synode_app_data(connection_descriptor *con,
                                       uint32_t group_id_hash,
                                       synode_no_array &synodes,
                                       synode_app_data_array &reply);

  bool xcom_client_set_cache_size(uint64_t size);
  bool xcom_client_boot(node_list *nl, uint32_t group_id);
  connection_descriptor *xcom_client_open_connection(std::string,
                                                     xcom_port port);
  bool xcom_client_close_connection(connection_descriptor *fd);
  bool xcom_client_send_data(unsigned long long size, char *data);
  void xcom_init(xcom_port listen_port);
  bool xcom_exit(bool xcom_input_open);
  int xcom_get_ssl_mode(const char *mode);
  int xcom_get_ssl_fips_mode(const char *mode);
  int xcom_set_ssl_fips_mode(int mode);
  int xcom_set_ssl_mode(int mode);
  bool xcom_init_ssl();
  void xcom_destroy_ssl();
  bool xcom_use_ssl();
  void xcom_set_ssl_parameters(const char *server_key_file,
                               const char *server_cert_file,
                               const char *client_key_file,
                               const char *client_cert_file,
                               const char *ca_file, const char *ca_path,
                               const char *crl_file, const char *crl_path,
                               const char *cipher, const char *tls_version);
  site_def const *find_site_def(synode_no synode);

  enum_gcs_error xcom_wait_ready();
  bool xcom_is_ready();
  void xcom_set_ready(bool value);
  void xcom_signal_ready();

  void xcom_wait_for_xcom_comms_status_change(int &status);
  bool xcom_has_comms_status_changed();
  void xcom_set_comms_status(int status);
  void xcom_signal_comms_status_changed(int status);

  enum_gcs_error xcom_wait_exit();
  bool xcom_is_exit();
  void xcom_set_exit(bool value);
  void xcom_signal_exit();

  void xcom_set_cleanup();

  bool xcom_client_force_config(node_list *nl, uint32_t group_id);
  bool get_should_exit();
  void set_should_exit(bool should_exit);

  bool xcom_input_connect();
  bool xcom_input_try_push(app_data_ptr data);
  Gcs_xcom_input_queue::future_reply xcom_input_try_push_and_get_reply(
      app_data_ptr data);
  xcom_input_request_ptr xcom_input_try_pop();

 private:
  /*
    Maximum waiting time used by timed_waits in xcom_wait_ready and
    xcom_wait_for_xcom_comms_status_change.
  */
  unsigned int m_wait_time;

  // For synchronization between XCom and MySQL GCS infrastructure at startup.
  My_xp_mutex_impl m_lock_xcom_ready;
  My_xp_cond_impl m_cond_xcom_ready;
  bool m_is_xcom_ready;

  My_xp_mutex_impl m_lock_xcom_comms_status;
  My_xp_cond_impl m_cond_xcom_comms_status;
  int m_xcom_comms_status;

  My_xp_mutex_impl m_lock_xcom_exit;
  My_xp_cond_impl m_cond_xcom_exit;
  bool m_is_xcom_exit;

  My_xp_socket_util *m_socket_util;

  // Stores SSL parameters
  const char *m_server_key_file;
  const char *m_server_cert_file;
  const char *m_client_key_file;
  const char *m_client_cert_file;
  const char *m_ca_file;
  const char *m_ca_path;
  const char *m_crl_file;
  const char *m_crl_path;
  const char *m_cipher;
  const char *m_tls_version;

  std::atomic_bool m_should_exit;

  /* Input channel to XCom */
  Gcs_xcom_input_queue m_xcom_input_queue;

  /*
    Disabling the copy constructor and assignment operator.
  */
  Gcs_xcom_proxy_impl(Gcs_xcom_proxy_impl const &);
  Gcs_xcom_proxy_impl &operator=(Gcs_xcom_proxy_impl const &);
};

/**
  A Gcs_xcom_interface needs to have an instance of this class
  initialized before engaging XCom.
 */
class Gcs_xcom_app_cfg {
 public:
  explicit Gcs_xcom_app_cfg() {}

  virtual ~Gcs_xcom_app_cfg() {}

  /**
    Initializes the data structures to communicate with
    XCom the application injected configuration options.
   */
  void init();

  /**
    Configures how many loops to spin before blocking on
    the poll system call.
    @param loops the number of spins.
   */
  void set_poll_spin_loops(unsigned int loops);

  /**
    Configures the maximum size of the xcom cache.
    @param size the maximum size of the cache.
   */
  void set_xcom_cache_size(uint64_t size);

  /**
   Configures XCom with its unique instance identifier, i.e. its (address,
   incarnation) pair.

   Takes ownership of @c identity.

   @param identity the unique identifier
   @retval true if there was an error configuring XCom
   @retval false if configuration was successful
   */
  bool set_identity(node_address *identity);

  /**
    Must be called when XCom is not engaged anymore.
   */
  void deinit();
};

struct Gcs_xcom_thread_startup_parameters {
  Gcs_xcom_proxy *proxy;
  unsigned int port;
};

#endif /* GCS_XCOM_PROXY_INCLUDED */
