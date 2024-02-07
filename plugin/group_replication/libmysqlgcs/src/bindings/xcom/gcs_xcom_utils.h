/* Copyright (c) 2015, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef GCS_XCOM_UTILS_INCLUDED
#define GCS_XCOM_UTILS_INCLUDED

#include <vector>
#include "plugin/group_replication/libmysqlgcs/include/mysql/gcs/gcs_group_identifier.h"
#include "plugin/group_replication/libmysqlgcs/include/mysql/gcs/gcs_types.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/network/include/network_provider.h"
#include "plugin/group_replication/libmysqlgcs/xdr_gen/xcom_vp.h"

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
  static uint32_t mhash(const unsigned char *buf, size_t length);

  static int init_net();
  static int deinit_net();

  virtual ~Gcs_xcom_utils();
};

/**
 * Converts the given GCS protocol version into the MySQL version that
 * introduced it.
 *
 * @param protocol the GCS protocol
 * @return the MySQL version that introduced it, in format major.minor.patch
 */
std::string gcs_protocol_to_mysql_version(Gcs_protocol_version protocol);

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
 * @brief Parses the string "host:port" and checks if it is correct.
 *
 * @param server_and_port the server hostname and port in the form
 * hostname:port.
 *
 * @return true if it is a valid URL, false otherwise.
 */
bool is_valid_hostname(const std::string &server_and_port);

/**
 Checks whether the given string is a valid GCS protocol known by this node.

 @returns true If it is, false otherwise.
 */
bool is_valid_protocol(std::string const &protocol);

/**
 Does some transformations on the parameters. For instance, replaces
 aliases with the correct ones
 */
void fix_parameters_syntax(Gcs_interface_parameters &params);

/**
 Checks that parameters are syntactically valid.

 @param params        The parameters to validate syntactically.
 @param netns_manager A reference to a Network Namespace Manager.
                      This is needed because of the allowlist configuration and
                      local address validation.

 @returns false if there is a syntax error, true otherwise.
 */
bool is_parameters_syntax_correct(const Gcs_interface_parameters &params,
                                  Network_namespace_manager *netns_manager);
#endif /* GCS_XCOM_UTILS_INCLUDED */
