/* Copyright (c) 2015, 2018, Oracle and/or its affiliates. All rights reserved.

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

#ifndef GCS_XCOM_UTILS_INCLUDED
#define GCS_XCOM_UTILS_INCLUDED

#include <atomic>
#include <string>
#include <vector>

#include "plugin/group_replication/libmysqlgcs/include/mysql/gcs/gcs_group_identifier.h"
#include "plugin/group_replication/libmysqlgcs/include/mysql/gcs/gcs_member_identifier.h"
#include "plugin/group_replication/libmysqlgcs/include/mysql/gcs/gcs_types.h"
#include "plugin/group_replication/libmysqlgcs/include/mysql/gcs/xplatform/my_xp_cond.h"
#include "plugin/group_replication/libmysqlgcs/include/mysql/gcs/xplatform/my_xp_mutex.h"
#include "plugin/group_replication/libmysqlgcs/include/mysql/gcs/xplatform/my_xp_thread.h"
#include "plugin/group_replication/libmysqlgcs/include/mysql/gcs/xplatform/my_xp_util.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/gcs_xcom_group_member_information.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/node_connection.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/node_list.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/node_set.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/server_struct.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/simset.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/site_def.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/site_struct.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/task.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/task_net.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/xcom_base.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/xcom_common.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/xcom_detector.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/xcom_transport.h"
#include "plugin/group_replication/libmysqlgcs/xdr_gen/xcom_vp.h"

#define XCOM_COMM_STATUS_UNDEFINED -1
#define XCOM_PREFIX "[XCOM] "

/**
  @class gcs_xcom_utils

  Class where the common binding utilities reside as static methods.
*/
class Gcs_xcom_utils {
 public:
  /**
    Create a xcom group identifier from a Group Identifier.

    @param[in] group_id A group identifier

    @return an hash of the group identifier string that will serve as input
            for the group id in XCom
  */
  static u_long build_xcom_group_id(Gcs_group_identifier &group_id);

  /**
    Processes a list of comma separated peer nodes.

    @param peer_nodes input string of comma separated peer nodes
    @param[out] processed_peers the list of configured peers
   */
  static void process_peer_nodes(const std::string *peer_nodes,
                                 std::vector<std::string> &processed_peers);

  /**
    Validates peer nodes according with IP/Address rules enforced by
    is_valid_hostname function

    @param [in,out] peers input list of peer nodes. It will be cleansed of
                    invalid peers
    @param [in,out] invalid_peers This list will contain all invalid peers.
   */
  static void validate_peer_nodes(std::vector<std::string> &peers,
                                  std::vector<std::string> &invalid_peers);

  /**
   Simple multiplicative hash.

   @param buf the data to create an hash from
   @param length data length

   @return calculated hash
   */
  static uint32_t mhash(unsigned char *buf, size_t length);

  static int init_net();
  static int deinit_net();

  virtual ~Gcs_xcom_utils();
};

/**
  @class gcs_xcom_control_proxy

  This class is an abstraction layer between xcom and the actual
  implementation. The purpose of this is to allow Gcs_xcom_control_interface
  to be unit testable by creating mock classes on top of it.
*/
class Gcs_xcom_proxy {
 public:
  const static int connection_attempts = 10;

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
  */

  virtual void delete_node_address(unsigned int n, node_address *na) = 0;

  /**
    This member function is responsible to call into XCom consensus and add a
    node to the group. The caller is responsible for ensuring that the session
    has been opened before @c open_session and also that the node is not yet
    in the configuration.

    The callee must have opened an xcom connection before calling this
    function. @c xcom_client_open_connection.

    @param fd the file descriptor to the XCom connection established earlier
    @param nl The node list containing the list of peers to add
    @param group_id the identifier of the group to which the nodes should
           be added

    @return false on success, true otherwise. There could be errors later in
    the process of adding a node. Since this is basically an asynchronous
    function, one needs to wait for the actual view change to validate that we
    were added to the configuration.
  */

  virtual int xcom_client_add_node(connection_descriptor *fd, node_list *nl,
                                   uint32_t group_id) = 0;

  /**
    This member function is responsible for triggering the removal of a node
    from the XCom configuration. This function is asynchronous, so you need to
    wait for the view change to actually validate that the removal was
    successful.

    The caller is responsible for making sure that the server to be removed is
    in the group.

    This function MUST be called after opening the local xcom session
    @c xcom_open_handlers.

    @param nl The list of nodes to remove from the group
    @param group_id The identifier of the group from which the nodes will
           be removed
  */

  virtual int xcom_client_remove_node(node_list *nl, uint32_t group_id) = 0;

  /**
    This member function is responsible for triggering the removal of a node
    from the XCom configuration. This function is asynchronous, so you need to
    wait for the view change to actually validate that the removal was
    successful.

    The caller is responsible for making sure that the server to be removed is
    in the group.

    This function MUST be called after opening the local xcom session
    @c xcom_open_handlers.

    This method is to contact remote instances requesting for a node
    to be removed. You can use the local version which is:
    - int xcom_client_remove_node(node_list *nl, uint32_t group_id)

    @param fd the file descriptor to the XCom connection established earlier
    @param nl The list of nodes to remove from the group
    @param group_id The identifier of the group from which the nodes will
           be removed
  */
  virtual int xcom_client_remove_node(connection_descriptor *fd, node_list *nl,
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

    @return true if successful and event_horizon was written to, false otherwise

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

    @return true if successful, false otherwise
   */
  virtual bool xcom_client_set_event_horizon(
      uint32_t group_id, xcom_event_horizon event_horizon) = 0;

  /**
    This member function is responsible for pushing data into consensus on
    XCom. The caller is responsible to making sure that there is an open XCom
    session @c xcom_open_handlers and also that the server is  part of the XCom
    configuration before sending data to it.

    @param size the size of the payload
    @param data the payload

    @return 0 on success, not 0 otherwise
  */

  virtual int xcom_client_send_data(unsigned long long size, char *data) = 0;

  /**
    This member function is used to send a 'die_op' to the local XCom. It is
    intended to test the fix for Bug#27918666 - Remove the exit call() from
    die_op in XCOM.
   */
  virtual int xcom_client_send_die() = 0;

  /**
    This member function initializes XCom. This function must be called before
    anything else and from within the XCom thread. It will eventually call
    the main loop inside XCom.

    @param listen_port the port that the local XCom is to be listening on.
  */

  virtual int xcom_init(xcom_port listen_port) = 0;

  /**
    This member function finishes the XCom thread. This function must be
    called when the XCOM thread was started but the node has not joined
    a group.

    @@param xcom_handlers_open indicates whether the handlers for the
                               xcom thread have already been opened.
  */

  virtual int xcom_exit(bool xcom_handlers_open) = 0;

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

  /*
    Initialize the SSL.
    Return 0 if success 1 otherwise.
  */

  virtual int xcom_init_ssl() = 0;

  /*
    Destroy the SSL Configuration freeing allocated memory.
  */

  virtual void xcom_destroy_ssl() = 0;

  /*
    Return whether the SSL will be used to encrypt data or not.

    Return 1 if it is enabled 0 otherwise.
  */

  virtual int xcom_use_ssl() = 0;

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
    @return false 0 on success, not 0 otherwise
  */

  virtual int xcom_client_boot(node_list *nl, uint32_t group_id) = 0;

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
    @return 0 on success, not 0 otherwise
  */

  virtual int xcom_client_close_connection(connection_descriptor *fd) = 0;

  /**
    This member function opens all the local connections to XCom needed before
    one can push data into it.

    Even though this is used to connect to a local XCom, it can be used to
    connect to a standalone XCom.

    @param saddr The XCom address
    @param port The XCom port

    @return false on success, true otherwise. If there was an error, no
    connection will have been created.
  */

  virtual bool xcom_open_handlers(std::string saddr, xcom_port port) = 0;

  /**
    This member function closes all the local connections to XCom that had been
    previously opened by open_session.

    @return false on success, true otherwise. If there was an error the state
    of the connections is undefined.
  */

  virtual bool xcom_close_handlers() = 0;

  /**
    Acquires one of the handlers opened through @c xcom_open_handlers and locks
    it.

    The caller is responsible for releasing the handler once it is not needed
    anymore.

    This function is mostly used by the locally connecting functions such as
    @c xcom_client_send_data, @c xcom_client_add_node,
    @c xcom_client_set_event_horizon, @c xcom_client_remove_node,
    @c xcom_client_boot.

    @return a valid handler to communicate with XCom
  */

  virtual int xcom_acquire_handler() = 0;

  /**
    Releases the handler and unlocks it.

    @param index The handler that was previously acquired
  */

  virtual void xcom_release_handler(int index) = 0;

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
   */
  virtual int xcom_client_force_config(node_list *nl, uint32_t group_id) = 0;

  /**
    This method forces XCom to inject a new configuration in the group,
    even if it does not contain a majority of members.

    @param fd the file descriptor to the XCom connection established earlier
    @param nl The list of nodes that will belong to this new configuration
    @param group_id The identifier of the group from which the nodes will
           belong
   */
  virtual int xcom_client_force_config(connection_descriptor *fd, node_list *nl,
                                       uint32_t group_id) = 0;

  /**
    Function used to boot a node in XCOM.

    @param node Node information.
    @param group_id_hash Hash of group identifier.
  */
  virtual int xcom_boot_node(Gcs_xcom_node_information &node,
                             uint32_t group_id_hash) = 0;

  /**
    Function to remove a set of nodes from XCOM.

    @param nodes Set of nodes.
    @param group_id_hash Hash of group identifier.
  */

  virtual int xcom_remove_nodes(Gcs_xcom_nodes &nodes,
                                uint32_t group_id_hash) = 0;

  /**
    Function to remove a set of nodes from XCOM.

    @param con Connection to a node that will carry on the request.
    @param nodes Set of nodes to remove.
    @param group_id_hash Hash of group identifier.
  */

  virtual int xcom_remove_nodes(connection_descriptor &con,
                                Gcs_xcom_nodes &nodes,
                                uint32_t group_id_hash) = 0;

  /**
    Function to remove a node from XCOM.

    @param node Node information.
    @param group_id_hash Hash of group identifier.
  */

  virtual int xcom_remove_node(const Gcs_xcom_node_information &node,
                               uint32_t group_id_hash) = 0;

  /**
    Function to add a set of nodes to XCOM.

    @param con Connection to a node that will carry on the request.
    @param nodes Set of nodes.
    @param group_id_hash Hash of group identifier.
  */

  virtual int xcom_add_nodes(connection_descriptor &con, Gcs_xcom_nodes &nodes,
                             uint32_t group_id_hash) = 0;

  /**
    Function to add a node to XCOM.

  @param con Connection to a node that will carry on the request.
  @param node Node information.
  @param group_id_hash Hash of group identifier.
  */

  virtual int xcom_add_node(connection_descriptor &con,
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
    Function to reconfigure XCOM's event horizon.

    @param group_id_hash Hash of group identifier.
    @param event_horizon Desired event horizon value.

    @return true if successful, false otherwise
  */
  virtual bool xcom_set_event_horizon(uint32_t group_id_hash,
                                      xcom_event_horizon event_horizon) = 0;

  /**
    Function to force the set of nodes in XCOM's configuration.

    @param nodes Set of nodes.
    @param group_id_hash Hash of group identifier.
  */

  virtual int xcom_force_nodes(Gcs_xcom_nodes &nodes,
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

  int xcom_boot_node(Gcs_xcom_node_information &node, uint32_t group_id_hash);
  int xcom_remove_nodes(Gcs_xcom_nodes &nodes, uint32_t group_id_hash);
  int xcom_remove_nodes(connection_descriptor &con, Gcs_xcom_nodes &nodes,
                        uint32_t group_id_hash);
  int xcom_remove_node(const Gcs_xcom_node_information &node,
                       uint32_t group_id_hash);
  int xcom_add_nodes(connection_descriptor &con, Gcs_xcom_nodes &nodes,
                     uint32_t group_id_hash);
  int xcom_add_node(connection_descriptor &con,
                    const Gcs_xcom_node_information &node,
                    uint32_t group_id_hash);
  xcom_event_horizon xcom_get_minimum_event_horizon();
  xcom_event_horizon xcom_get_maximum_event_horizon();
  bool xcom_get_event_horizon(uint32_t group_id_hash,
                              xcom_event_horizon &event_horizon);
  bool xcom_set_event_horizon(uint32_t group_id_hash,
                              xcom_event_horizon event_horizon);
  int xcom_force_nodes(Gcs_xcom_nodes &nodes, uint32_t group_id_hash);

 private:
  /* Serialize information on nodes to be sent to XCOM */
  bool serialize_nodes_information(Gcs_xcom_nodes &nodes, node_list &nl);

  /* Free information on nodes sent to XCOM */
  void free_nodes_information(node_list &nl);
};

/**
  @class gcs_xcom_control_proxy_impl

  Implementation of gcs_xcom_control_proxy to be used by whom
  instantiates Gcs_xcom_control_interface to be used in a real
  scenario.
*/
class Gcs_xcom_proxy_impl : public Gcs_xcom_proxy_base {
 private:
  class Xcom_handler {
   private:
    My_xp_mutex_impl m_lock;
    connection_descriptor *m_fd;

   public:
    explicit Xcom_handler();
    virtual ~Xcom_handler();

    connection_descriptor *get_fd() { return m_fd; }
    void set_fd(connection_descriptor *fd) { m_fd = fd; }
    void lock() { m_lock.lock(); }
    void unlock() { m_lock.unlock(); }

   private:
    /*
      Disabling the copy constructor and assignment operator.
    */
    Xcom_handler(Xcom_handler const &);
    Xcom_handler &operator=(Xcom_handler const &);
  };

 public:
  explicit Gcs_xcom_proxy_impl();
  Gcs_xcom_proxy_impl(unsigned int wt);
  virtual ~Gcs_xcom_proxy_impl();

  node_address *new_node_address_uuid(unsigned int n, char *names[],
                                      blob uuids[]);
  void delete_node_address(unsigned int n, node_address *na);
  int xcom_client_add_node(connection_descriptor *fd, node_list *nl,
                           uint32_t group_id);
  int xcom_client_remove_node(connection_descriptor *fd, node_list *nl,
                              uint32_t group_id);
  int xcom_client_remove_node(node_list *nl, uint32_t group_id);
  bool xcom_client_get_event_horizon(uint32_t group_id,
                                     xcom_event_horizon &event_horizon);
  bool xcom_client_set_event_horizon(uint32_t group_id,
                                     xcom_event_horizon event_horizon);
  int xcom_client_boot(node_list *nl, uint32_t group_id);
  connection_descriptor *xcom_client_open_connection(std::string,
                                                     xcom_port port);
  int xcom_client_close_connection(connection_descriptor *fd);
  int xcom_client_send_data(unsigned long long size, char *data);
  int xcom_client_send_die();
  int xcom_init(xcom_port listen_port);
  int xcom_exit(bool xcom_handlers_open);
  int xcom_get_ssl_mode(const char *mode);
  int xcom_get_ssl_fips_mode(const char *mode);
  int xcom_set_ssl_fips_mode(int mode);
  int xcom_set_ssl_mode(int mode);
  int xcom_init_ssl();
  void xcom_destroy_ssl();
  int xcom_use_ssl();
  void xcom_set_ssl_parameters(const char *server_key_file,
                               const char *server_cert_file,
                               const char *client_key_file,
                               const char *client_cert_file,
                               const char *ca_file, const char *ca_path,
                               const char *crl_file, const char *crl_path,
                               const char *cipher, const char *tls_version);
  site_def const *find_site_def(synode_no synode);

  bool xcom_open_handlers(std::string saddr, xcom_port port);
  bool xcom_close_handlers();
  int xcom_acquire_handler();
  void xcom_release_handler(int index);
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

  int xcom_client_force_config(node_list *nl, uint32_t group_id);
  int xcom_client_force_config(connection_descriptor *fd, node_list *nl,
                               uint32_t group_id);
  bool get_should_exit();
  void set_should_exit(bool should_exit);

 private:
  /* A pointer to the next local XCom connection to use. */
  int m_xcom_handlers_cursor;

  /* This lock protects the list of local XCom connections. */
  My_xp_mutex_impl m_lock_xcom_cursor;

  /* The maximum number of local xcom connections. */
  int m_xcom_handlers_size;

  /*
    Maximum waiting time used by timed_waits in xcom_wait_ready and
    xcom_wait_for_xcom_comms_status_change.
  */
  unsigned int m_wait_time;

  /* A list of local XCom connections. */
  Xcom_handler **m_xcom_handlers;

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
    Must be called when XCom is not engaged anymore.
   */
  void deinit();
};

struct Gcs_xcom_thread_startup_parameters {
  Gcs_xcom_proxy *proxy;
  unsigned int port;
};

/*****************************************************
 *****************************************************

 Auxiliary checking functions.

 *****************************************************
 *****************************************************
 */

/**
 Checks whether the given string is a number or not
 @param s the string to check.
 @return true if it is a number, false otherwise.
 */
inline bool is_number(const std::string &s) {
  return s.find_first_not_of("0123456789") == std::string::npos;
}

/**
 Parses the string "host:port" and checks if it is correct.

 @param server_and_port the server hostname and port in the form hostname:port.
 @return true if it is a valid URL, false otherwise.
 */
bool is_valid_hostname(const std::string &server_and_port);

/**
 Does some transformations on the parameters. For instance, replaces
 aliases with the correct ones
 */
void fix_parameters_syntax(Gcs_interface_parameters &params);

/**
 Checks that parameters are syntactically valid.

 @param params The parameters to validate syntactically.
 @returns false if there is a syntax error, true otherwise.
 */
bool is_parameters_syntax_correct(const Gcs_interface_parameters &params);
#endif /* GCS_XCOM_UTILS_INCLUDED */
