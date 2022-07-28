/* Copyright (c) 2015, 2022, Oracle and/or its affiliates.

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

#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/gcs_xcom_utils.h"

#include <algorithm>
#include <cerrno>     // errno
#include <cinttypes>  // std::strtoumax
#include <cstdlib>
#include <sstream>
#ifndef _WIN32
#include <netdb.h>
#include <sys/socket.h>
#endif
#include "plugin/group_replication/libmysqlgcs/include/mysql/gcs/gcs_logging_system.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/gcs_message_stage_lz4.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/gcs_message_stage_split.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/gcs_xcom_networking.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/xcom_transport.h"

/*
  Time is defined in seconds.
*/
static const unsigned int WAITING_TIME = 30;

/*
  Number of attempts to join a group.
*/
static const unsigned int JOIN_ATTEMPTS = 0;

/*
  Sleep time between attempts defined in seconds.
*/
static const uint64_t JOIN_SLEEP_TIME = 5;

/*
  Default value for the maximum size of the XCom cache.
*/
static const uint64_t DEFAULT_XCOM_MAX_CACHE_SIZE = 1073741824;

/*
  Min value for the maximum size of the XCom cache.
*/
static const uint64_t MIN_XCOM_MAX_CACHE_SIZE = 134217728;

Gcs_xcom_utils::~Gcs_xcom_utils() = default;

u_long Gcs_xcom_utils::build_xcom_group_id(Gcs_group_identifier &group_id) {
  std::string group_id_str = group_id.get_group_id();
  return mhash(static_cast<const unsigned char *>(
                   static_cast<const void *>(group_id_str.c_str())),
               group_id_str.size());
}

void Gcs_xcom_utils::process_peer_nodes(
    const std::string *peer_nodes, std::vector<std::string> &processed_peers) {
  std::string peer_init(peer_nodes->c_str());
  std::string delimiter = ",";

  // Clear all whitespace in the string
  peer_init.erase(std::remove(peer_init.begin(), peer_init.end(), ' '),
                  peer_init.end());

  // Skip delimiter at beginning.
  std::string::size_type lastPos = peer_init.find_first_not_of(delimiter, 0);

  // Find first "non-delimiter".
  std::string::size_type pos = peer_init.find_first_of(delimiter, lastPos);

  while (std::string::npos != pos || std::string::npos != lastPos) {
    std::string peer(peer_init.substr(lastPos, pos - lastPos));
    processed_peers.push_back(peer);

    // Skip delimiter
    lastPos = peer_init.find_first_not_of(delimiter, pos);

    // Find next "non-delimiter"
    pos = peer_init.find_first_of(delimiter, lastPos);
  }
}

void Gcs_xcom_utils::validate_peer_nodes(
    std::vector<std::string> &peers, std::vector<std::string> &invalid_peers) {
  std::vector<std::string>::iterator it;
  for (it = peers.begin(); it != peers.end();) {
    std::string server_and_port = *it;
    if (!is_valid_hostname(server_and_port)) {
      invalid_peers.push_back(server_and_port);
      it = peers.erase(it);
    } else {
      ++it;
    }
  }
}

uint32_t Gcs_xcom_utils::mhash(const unsigned char *buf, size_t length) {
  size_t i = 0;
  uint32_t sum = 0;
  for (i = 0; i < length; i++) {
    sum += 0x811c9dc5 * (uint32_t)buf[i];
  }

  return sum;
}

int Gcs_xcom_utils::init_net() { return ::init_net(); }

int Gcs_xcom_utils::deinit_net() { return ::deinit_net(); }

bool is_valid_hostname(const std::string &server_and_port) {
  char hostname[IP_MAX_SIZE];
  xcom_port port = 0;
  bool error = false;
  struct addrinfo *addr = nullptr;

  if ((error = get_ip_and_port(const_cast<char *>(server_and_port.c_str()),
                               hostname, &port))) {
    goto end;
  }

  /* handle hostname*/
  error = (checked_getaddrinfo(hostname, nullptr, nullptr, &addr) != 0);
  if (error) goto end;

end:
  if (addr) freeaddrinfo(addr);
  return error == false;
}

void fix_parameters_syntax(Gcs_interface_parameters &interface_params) {
  std::string *compression_str =
      const_cast<std::string *>(interface_params.get_parameter("compression"));
  std::string *compression_threshold_str = const_cast<std::string *>(
      interface_params.get_parameter("compression_threshold"));
  std::string *wait_time_str =
      const_cast<std::string *>(interface_params.get_parameter("wait_time"));
  std::string *ip_allowlist_str =
      const_cast<std::string *>(interface_params.get_parameter("ip_allowlist"));
  std::string *ip_allowlist_reconfigure_str = const_cast<std::string *>(
      interface_params.get_parameter("reconfigure_ip_allowlist"));
  std::string *join_attempts_str = const_cast<std::string *>(
      interface_params.get_parameter("join_attempts"));
  std::string *join_sleep_time_str = const_cast<std::string *>(
      interface_params.get_parameter("join_sleep_time"));
  std::string *fragmentation_str = const_cast<std::string *>(
      interface_params.get_parameter("fragmentation"));
  std::string *fragmentation_threshold_str = const_cast<std::string *>(
      interface_params.get_parameter("fragmentation_threshold"));
  std::string *xcom_cache_size_str = const_cast<std::string *>(
      interface_params.get_parameter("xcom_cache_size"));
  std::string *communication_stack_str = const_cast<std::string *>(
      interface_params.get_parameter("communication_stack"));

  // Sets the default value for the communication stack to use
  if (!communication_stack_str) {  // Default is XCom...
    interface_params.add_parameter("communication_stack",
                                   std::to_string(XCOM_PROTOCOL));
  }

  // sets the default value for compression (ON by default)
  if (!compression_str) {
    interface_params.add_parameter("compression", "on");
  }

  // sets the default threshold if no threshold has been set
  if (!compression_threshold_str) {
    std::stringstream ss;
    ss << Gcs_message_stage_lz4::DEFAULT_THRESHOLD;
    interface_params.add_parameter("compression_threshold", ss.str());
  }

  // sets the default waiting time for timed_waits
  if (!wait_time_str) {
    std::stringstream ss;
    ss << WAITING_TIME;
    interface_params.add_parameter("wait_time", ss.str());
  }

  bool should_configure_allowlist = true;
  if (ip_allowlist_reconfigure_str) {
    should_configure_allowlist =
        ip_allowlist_reconfigure_str->compare("on") == 0 ||
        ip_allowlist_reconfigure_str->compare("true") == 0;
  }

  // sets the default ip allowlist
  if (should_configure_allowlist && !ip_allowlist_str) {
    std::stringstream ss;
    std::string iplist;
    std::map<std::string, int> out;

    // add local private networks that one has an IP on by default
    get_local_private_addresses(out);

    if (out.empty())
      ss << "127.0.0.1/32,::1/128,";
    else {
      std::map<std::string, int>::iterator it;
      for (it = out.begin(); it != out.end(); it++) {
        ss << (*it).first << "/" << (*it).second << ",";
      }
    }

    iplist = ss.str();
    iplist.erase(iplist.end() - 1);  // remove trailing comma

    MYSQL_GCS_LOG_INFO("Added automatically IP ranges " << iplist
                                                        << " to the allowlist");

    interface_params.add_parameter("ip_allowlist", iplist);
  }

  // sets the default join attempts
  if (!join_attempts_str) {
    std::stringstream ss;
    ss << JOIN_ATTEMPTS;
    interface_params.add_parameter("join_attempts", ss.str());
  }

  // sets the default sleep time between join attempts
  if (!join_sleep_time_str) {
    std::stringstream ss;
    ss << JOIN_SLEEP_TIME;
    interface_params.add_parameter("join_sleep_time", ss.str());
  }

  // sets the default value for fragmentation (ON by default)
  if (!fragmentation_str) {
    interface_params.add_parameter("fragmentation", "on");
  }

  // sets the default threshold if no threshold has been set
  if (!fragmentation_threshold_str) {
    std::stringstream ss;
    ss << Gcs_message_stage_split_v2::DEFAULT_THRESHOLD;
    interface_params.add_parameter("fragmentation_threshold", ss.str());
  }

  // sets the default XCom cache size
  if (!xcom_cache_size_str) {
    interface_params.add_parameter("xcom_cache_size",
                                   std::to_string(DEFAULT_XCOM_MAX_CACHE_SIZE));
  }
}

static enum_gcs_error is_valid_flag(const std::string param,
                                    std::string &flag) {
  enum_gcs_error error = GCS_OK;

  // transform to lower case
  std::transform(flag.begin(), flag.end(), flag.begin(), ::tolower);

  if (flag.compare("on") && flag.compare("off") && flag.compare("true") &&
      flag.compare("false")) {
    std::stringstream ss;
    ss << "Invalid parameter set to " << param << ". ";
    ss << "Valid values are either \"on\" or \"off\".";
    MYSQL_GCS_LOG_ERROR(ss.str());
    error = GCS_NOK;
  }
  return error;
}

bool is_valid_protocol(std::string const &protocol_string) {
  int constexpr BASE_10 = 10;
  bool constexpr VALID = true;
  bool constexpr INVALID = false;
  bool result = INVALID;
  char const *protocol_c_str = protocol_string.c_str();
  std::uintmax_t protocol_number = 0;
  bool couldnt_convert = true;
  bool out_of_range = true;
  char *end = nullptr;
  Gcs_protocol_version protocol = Gcs_protocol_version::UNKNOWN;

  if (!is_number(protocol_string)) goto end;

  // Try to convert.
  errno = 0;
  protocol_number = std::strtoumax(protocol_c_str, &end, BASE_10);
  couldnt_convert = (protocol_c_str == end);
  out_of_range = (errno == ERANGE);
  if (couldnt_convert || out_of_range) {
    if (out_of_range) errno = 0;
    goto end;
  }

  // Confirm protocol is within the domain [1; max-protocol-known].
  protocol = static_cast<Gcs_protocol_version>(protocol_number);
  if (protocol < Gcs_protocol_version::V1 ||
      protocol > Gcs_protocol_version::HIGHEST_KNOWN) {
    goto end;
  }

  result = VALID;
end:
  return result;
}

bool is_parameters_syntax_correct(
    const Gcs_interface_parameters &interface_params,
    Network_namespace_manager *netns_manager) {
  enum_gcs_error error = GCS_OK;
  Gcs_sock_probe_interface *sock_probe_interface =
      new Gcs_sock_probe_interface_impl();

  // get the parameters
  const std::string *group_name_str =
      interface_params.get_parameter("group_name");
  const std::string *local_node_str =
      interface_params.get_parameter("local_node");
  const std::string *peer_nodes_str =
      interface_params.get_parameter("peer_nodes");
  const std::string *bootstrap_group_str =
      interface_params.get_parameter("bootstrap_group");
  const std::string *poll_spin_loops_str =
      interface_params.get_parameter("poll_spin_loops");
  const std::string *compression_threshold_str =
      interface_params.get_parameter("compression_threshold");
  const std::string *compression_str =
      interface_params.get_parameter("compression");
  const std::string *wait_time_str =
      interface_params.get_parameter("wait_time");
  const std::string *join_attempts_str =
      interface_params.get_parameter("join_attempts");
  const std::string *join_sleep_time_str =
      interface_params.get_parameter("join_sleep_time");
  const std::string *non_member_expel_timeout_str =
      interface_params.get_parameter("non_member_expel_timeout");
  const std::string *suspicions_processing_period_str =
      interface_params.get_parameter("suspicions_processing_period");
  const std::string *member_expel_timeout_str =
      interface_params.get_parameter("member_expel_timeout");
  const std::string *reconfigure_ip_allowlist_str =
      interface_params.get_parameter("reconfigure_ip_allowlist");
  const std::string *fragmentation_threshold_str =
      interface_params.get_parameter("fragmentation_threshold");
  const std::string *fragmentation_str =
      interface_params.get_parameter("fragmentation");
  const std::string *xcom_cache_size_str =
      interface_params.get_parameter("xcom_cache_size");
  const std::string *communication_stack_str =
      interface_params.get_parameter("communication_stack");

  /*
    -----------------------------------------------------
    Checks
    -----------------------------------------------------
   */

  // validate group name
  if (group_name_str != nullptr && group_name_str->size() == 0) {
    MYSQL_GCS_LOG_ERROR("The group_name parameter (" << group_name_str << ")"
                                                     << " is not valid.")
    error = GCS_NOK;
    goto end;
  }

  // validate bootstrap string
  // accepted values: true, false, on, off
  if (bootstrap_group_str != nullptr) {
    std::string &flag = const_cast<std::string &>(*bootstrap_group_str);
    error = is_valid_flag("bootstrap_group", flag);
    if (error == GCS_NOK) goto end;
  }

  // validate peer addresses addresses
  if (peer_nodes_str != nullptr) {
    /*
     Parse and validate hostname and ports.
     */
    std::vector<std::string> hostnames_and_ports;
    std::vector<std::string> invalid_hostnames_and_ports;
    Gcs_xcom_utils::process_peer_nodes(peer_nodes_str, hostnames_and_ports);
    Gcs_xcom_utils::validate_peer_nodes(hostnames_and_ports,
                                        invalid_hostnames_and_ports);

    if (!invalid_hostnames_and_ports.empty()) {
      std::vector<std::string>::iterator invalid_hostnames_and_ports_it;
      for (invalid_hostnames_and_ports_it = invalid_hostnames_and_ports.begin();
           invalid_hostnames_and_ports_it != invalid_hostnames_and_ports.end();
           ++invalid_hostnames_and_ports_it) {
        MYSQL_GCS_LOG_WARN("Peer address \""
                           << (*invalid_hostnames_and_ports_it).c_str()
                           << "\" is not valid.");
      }
    }

    /*
     This means that none of the provided hosts is valid and that
     hostnames_and_ports had some sort of value
     */
    if (!invalid_hostnames_and_ports.empty() && hostnames_and_ports.empty()) {
      MYSQL_GCS_LOG_ERROR("None of the provided peer address is valid.");
      error = GCS_NOK;
      goto end;
    }
  }

  // Communication Stack
  if (communication_stack_str && (communication_stack_str->size() == 0 ||
                                  !is_number(*communication_stack_str))) {
    MYSQL_GCS_LOG_ERROR("The Commmunication Stack parameter ("
                        << communication_stack_str << ") is not valid.")
    error = GCS_NOK;
    goto end;
  }

  // local peer address
  if (local_node_str != nullptr) {
    bool matches_local_ip = false;
    std::map<std::string, int> ips, namespace_ips;
    std::map<std::string, int>::iterator it;

    char host_str[IP_MAX_SIZE];
    xcom_port local_node_port = 0;
    if (get_ip_and_port(const_cast<char *>(local_node_str->c_str()), host_str,
                        &local_node_port)) {
      MYSQL_GCS_LOG_ERROR("Invalid hostname or IP address ("
                          << *local_node_str << ") assigned to the parameter "
                          << "local_node!");
      error = GCS_NOK;
      goto end;
    }

    std::string host(host_str);
    std::vector<std::string> ip;
    int configured_protocol;
    std::string net_namespace;
    // first validate hostname
    if (!is_valid_hostname(*local_node_str)) {
      MYSQL_GCS_LOG_ERROR("Invalid hostname or IP address ("
                          << *local_node_str << ") assigned to the parameter "
                          << "local_node!");

      error = GCS_NOK;
      goto end;
    }

    // hostname was validated already, lets find the IP
    if (resolve_ip_addr_from_hostname(host, ip)) {
      MYSQL_GCS_LOG_ERROR("Unable to translate hostname " << host
                                                          << " to IP address!");
      error = GCS_NOK;
      goto end;
    }

    for (auto &ip_entry : ip) {
      if (ip_entry.compare(host) != 0)
        MYSQL_GCS_LOG_INFO("Translated '" << host << "' to "
                                          << ip_entry.c_str());
    }

    // second check that this host has that IP assigned
    if (get_local_addresses(*sock_probe_interface, ips, true)) {
      MYSQL_GCS_LOG_ERROR(
          "Unable to get the list of local IP addresses for "
          "the server!");
      error = GCS_NOK;
      goto end;
    }

    // see if any IP matches fromt he root namespace
    for (it = ips.begin(); it != ips.end() && !matches_local_ip; it++) {
      for (auto &ip_entry : ip) {
        matches_local_ip = (*it).first.compare(ip_entry) == 0;

        if (matches_local_ip) break;
      }
    }

    // If this server is configured to use a MySQL connection, we must check
    //  if we have a network namespace configured. If so, we must also check
    //  if the address exists in the network namespace
    configured_protocol = std::stoi(*communication_stack_str);
    /* purecov: begin deadcode */
    if (!matches_local_ip && netns_manager &&
        configured_protocol > XCOM_PROTOCOL) {
      // Check if we have a namespace configured
      netns_manager->channel_get_network_namespace(net_namespace);
      if (!net_namespace.empty()) {  // If the namespace is configured
        netns_manager->set_network_namespace(net_namespace);

        // second check that this host has that IP assigned in a namespace
        //  Use all interfaces, active or not, that contain an IP address
        if (get_local_addresses(*sock_probe_interface, namespace_ips)) {
          MYSQL_GCS_LOG_ERROR(
              "Unable to get the list of local IP addresses for "
              "the server!");
          error = GCS_NOK;
          netns_manager->restore_original_network_namespace();
          goto end;
        }

        // see if any IP matches fromt he root namespace
        for (it = namespace_ips.begin();
             it != namespace_ips.end() && !matches_local_ip; it++) {
          for (auto &ip_entry : ip) {
            matches_local_ip = (*it).first.compare(ip_entry) == 0;

            if (matches_local_ip) break;
          }
        }
        netns_manager->restore_original_network_namespace();
      }
    }
    /* purecov: end */
    if (!matches_local_ip) {
      MYSQL_GCS_LOG_ERROR(
          "There is no local IP address matching the one "
          "configured for the local node ("
          << *local_node_str << ").");
      error = GCS_NOK;
      goto end;
    }
  }

  // poll spin loops
  if (poll_spin_loops_str &&
      (poll_spin_loops_str->size() == 0 || !is_number(*poll_spin_loops_str))) {
    MYSQL_GCS_LOG_ERROR("The poll_spin_loops parameter (" << poll_spin_loops_str
                                                          << ") is not valid.")
    error = GCS_NOK;
    goto end;
  }

  // validate compression
  if (compression_str != nullptr) {
    std::string &flag = const_cast<std::string &>(*compression_str);
    error = is_valid_flag("compression", flag);
    if (error == GCS_NOK) goto end;
  }

  if (compression_threshold_str && (compression_threshold_str->size() == 0 ||
                                    !is_number(*compression_threshold_str))) {
    MYSQL_GCS_LOG_ERROR("The compression_threshold parameter ("
                        << compression_threshold_str << ") is not valid.")
    error = GCS_NOK;
    goto end;
  }

  if (wait_time_str &&
      (wait_time_str->size() == 0 || !is_number(*wait_time_str))) {
    MYSQL_GCS_LOG_ERROR("The wait_time parameter (" << wait_time_str
                                                    << ") is not valid.")
    error = GCS_NOK;
    goto end;
  }

  if (join_attempts_str &&
      (join_attempts_str->size() == 0 || !is_number(*join_attempts_str))) {
    MYSQL_GCS_LOG_ERROR("The join_attempts parameter (" << join_attempts_str
                                                        << ") is not valid.")
    error = GCS_NOK;
    goto end;
  }

  // validate suspicions parameters
  if (non_member_expel_timeout_str &&
      (non_member_expel_timeout_str->size() == 0 ||
       !is_number(*non_member_expel_timeout_str))) {
    MYSQL_GCS_LOG_ERROR("The non_member_expel_timeout parameter ("
                        << non_member_expel_timeout_str << ") is not valid.")
    error = GCS_NOK;
    goto end;
  }

  if (join_sleep_time_str &&
      (join_sleep_time_str->size() == 0 || !is_number(*join_sleep_time_str))) {
    MYSQL_GCS_LOG_ERROR("The join_sleep_time parameter (" << join_sleep_time_str
                                                          << ") is not valid.")
    error = GCS_NOK;
    goto end;
  }

  if (suspicions_processing_period_str &&
      (suspicions_processing_period_str->size() == 0 ||
       !is_number(*suspicions_processing_period_str))) {
    MYSQL_GCS_LOG_ERROR("The suspicions_processing_period parameter ("
                        << suspicions_processing_period_str
                        << ") is not valid.")
    error = GCS_NOK;
    goto end;
  }

  if (member_expel_timeout_str && (member_expel_timeout_str->size() == 0 ||
                                   !is_number(*member_expel_timeout_str))) {
    MYSQL_GCS_LOG_ERROR("The member_expel_timeout parameter ("
                        << member_expel_timeout_str << ") is not valid.")
    error = GCS_NOK;
    goto end;
  }

  // Validate allowlist reconfiguration parameter
  if (reconfigure_ip_allowlist_str != nullptr) {
    std::string &flag =
        const_cast<std::string &>(*reconfigure_ip_allowlist_str);
    error = is_valid_flag("reconfigure_ip_allowlist", flag);
    if (error == GCS_NOK) goto end;
  }

  // validate fragmentation
  if (fragmentation_str != nullptr) {
    std::string &flag = const_cast<std::string &>(*fragmentation_str);
    error = is_valid_flag("fragmentation", flag);
    if (error == GCS_NOK) goto end;
  }

  if (fragmentation_threshold_str &&
      (fragmentation_threshold_str->size() == 0 ||
       !is_number(*fragmentation_threshold_str))) {
    MYSQL_GCS_LOG_ERROR("The fragmentation_threshold parameter ("
                        << fragmentation_threshold_str << ") is not valid.")
    error = GCS_NOK;
    goto end;
  }

  // Validate XCom cache size
  errno = 0;
  if (xcom_cache_size_str != nullptr &&
      // Verify if the input value is a valid number
      (xcom_cache_size_str->size() == 0 || !is_number(*xcom_cache_size_str) ||
       // Check that it is not lower than the min value allowed for the var
       (strtoull(xcom_cache_size_str->c_str(), nullptr, 10) <
        MIN_XCOM_MAX_CACHE_SIZE) ||
       // Check that it is not higher than the max value allowed
       (strtoull(xcom_cache_size_str->c_str(), nullptr, 10) > ULONG_MAX) ||
       // Check that it is within the range of values allowed for the var
       // type. This is need in addition to the check above because of
       // overflows.
       errno == ERANGE)) {
    MYSQL_GCS_LOG_ERROR("The xcom_cache_size parameter ("
                        << xcom_cache_size_str->c_str() << ") is not valid.")
    error = GCS_NOK;
    goto end;
  }

end:
  delete sock_probe_interface;
  return error == GCS_NOK ? false : true;
}

std::string gcs_protocol_to_mysql_version(Gcs_protocol_version protocol) {
  std::string version;
  switch (protocol) {
    case Gcs_protocol_version::V1:
      version = "5.7.14";
      break;
    case Gcs_protocol_version::V2:
      version = "8.0.16";
      break;
    case Gcs_protocol_version::HIGHEST_KNOWN:
      version = "8.0.27";
      break;
    case Gcs_protocol_version::UNKNOWN:
    case Gcs_protocol_version::V4:
    case Gcs_protocol_version::V5:
      /* This should not happen... */
      break;
  }
  return version;
}
