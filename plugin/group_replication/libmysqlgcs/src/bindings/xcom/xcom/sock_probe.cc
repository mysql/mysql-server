/* Copyright (c) 2012, 2026, Oracle and/or its affiliates.

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

#ifndef _WIN32
#include <netdb.h>
#endif
#include <cstdlib>
#include <string_view>
#ifdef _MSC_VER
#include <stdint.h>
#endif

#include "xcom/node_no.h"
#include "xcom/server_struct.h"
#include "xcom/simset.h"
#include "xcom/site_struct.h"
#include "xcom/task.h"
#include "xcom/x_platform.h"
#include "xcom/xcom_cfg.h"
#include "xcom/xcom_detector.h"
#include "xcom/xcom_memory.h"
#include "xcom/xcom_profile.h"
#include "xcom/xcom_scope_guard.h"
#include "xdr_gen/xcom_vp.h"

#ifdef _WIN32
#include "xcom/sock_probe_win32.h"
#else
#include "xcom/sock_probe_ix.h"
#endif

/* compare two sockaddr */
bool_t sockaddr_default_eq(struct sockaddr *x, struct sockaddr *y) {
  size_t size_to_compare;
  if (x->sa_family != y->sa_family) return 0;

  size_to_compare = x->sa_family == AF_INET ? sizeof(struct sockaddr_in)
                                            : sizeof(struct sockaddr_in6);

  return 0 == memcmp(x, y, size_to_compare);
}

/* return index of this machine in node list, or -1 if no match */

static port_matcher match_port;
void set_port_matcher(port_matcher x) { match_port = x; }

port_matcher get_port_matcher() { return match_port; }

static inline struct addrinfo *probe_get_addrinfo(const char *name) {
#ifdef XCOM_STANDALONE
  return xcom_caching_getaddrinfo(name);
#else
  {
    struct addrinfo *addr = nullptr;
    checked_getaddrinfo(name, nullptr, nullptr, &addr);
    return addr;
  }
#endif
}

static inline void probe_free_addrinfo(struct addrinfo *addr) {
#ifdef XCOM_STANDALONE
  (void)addr;
#else
  if (addr) freeaddrinfo(addr);
#endif
}

/**
  Check whether an explicit XCom identity is currently configured.

  An identity is considered configured only when cfg_app_xcom_get_identity()
  returns a node address and that node address contains a non-null endpoint
  string.

  @retval true XCom identity is configured and contains an address.
  @retval false XCom identity is not configured or does not contain an
                address.
*/
static inline bool is_xcom_identity_configured() {
  node_address *identity = cfg_app_xcom_get_identity();
  return identity != nullptr && identity->address != nullptr;
}

/**
  Check whether one resolved address matches a valid local interface in the
  current network context.

  This helper compares a single resolved socket address against the interfaces
  currently visible through the supplied sock_probe. When a configured network
  namespace is active, all interfaces in the probe are considered eligible;
  otherwise, only interfaces reported as active are accepted.

  This helper assumes the caller has already switched to the intended network
  namespace, if any, before creating the sock_probe passed here.

  @param[in] s The socket probe snapshot that exposes the local interfaces to
               check against.
  @param[in] candidate_addr The resolved socket address to validate as local.
  @param[in] using_net_ns Whether the caller is running inside a configured
                          network namespace. When true, interfaces in the probe
                          are not filtered by active state.

  @retval true The resolved address matches a valid local interface in the
               current network context.
  @retval false The resolved address is null, does not match any visible local
                interface, or only matches interfaces that are not valid for
                the current network context.
*/
static bool is_address_on_valid_local_interface(sock_probe *s,
                                                struct sockaddr *candidate_addr,
                                                bool using_net_ns) {
  if (candidate_addr == nullptr) return false;

  for (int i = 0; i < number_of_interfaces(s); i++) {
    struct sockaddr *local_addr = nullptr;
    get_sockaddr_address(s, i, &local_addr);

    bool const interface_is_valid = using_net_ns ? true : is_if_running(s, i);
    if (local_addr != nullptr &&
        sockaddr_default_eq(candidate_addr, local_addr) && interface_is_valid) {
      return true;
    }
  }

  return false;
}

/**
  Check whether any resolved address in an addrinfo list matches a valid local
  interface in the current network context.

  This helper iterates over all addresses in the supplied addrinfo chain and
  applies is_address_on_valid_local_interface() to each entry until one valid
  local match is found.

  This helper assumes the caller has already switched to the intended network
  namespace, if any, before creating the sock_probe passed here.

  @param[in] s The socket probe snapshot that exposes the local interfaces to
               check against.
  @param[in] candidate_addr The head of the resolved addrinfo list to
                            validate.
  @param[in] using_net_ns Whether the caller is running inside a configured
                          network namespace. When true, interfaces in the probe
                          are not filtered by active state.

  @retval true At least one resolved address in the list matches a valid local
               interface in the current network context.
  @retval false No resolved address in the list matches a valid local
                interface, or the list is empty.
*/
static bool has_address_on_valid_local_interface(
    sock_probe *s, struct addrinfo *candidate_addr, bool using_net_ns) {
  while (candidate_addr != nullptr) {
    if (is_address_on_valid_local_interface(s, candidate_addr->ai_addr,
                                            using_net_ns)) {
      return true;
    }
    candidate_addr = candidate_addr->ai_next;
  }

  return false;
}

/**
  Check whether a candidate endpoint is the configured XCom identity in the
  current network context.

  When XCom identity is configured, self-detection must use that configured
  endpoint instead of relying only on matching any local interface on the same
  port. Both the candidate endpoint and the configured identity may identify
  their host part either as a hostname or as an IP address. The configured
  identity is read from cfg_app_xcom_get_identity() and split into its host/IP
  part and port.

  Matching is performed only when the candidate port equals the configured
  identity port. The host/IP parts are then compared in two stages:
  first, by direct textual comparison of the candidate host/IP part against the
  configured identity host/IP part; second, if the textual comparison does not
  match, by resolving both names and checking whether any resolved socket
  address is shared by both endpoints.

  After an endpoint match is established, the configured identity address must
  still be validated against the local interfaces visible in the current
  network namespace.

  This helper assumes the caller has already switched to the intended network
  namespace, if any, before creating the sock_probe passed here.

  @param[in] s The socket probe snapshot used to validate that the matched
               address exists on a valid local interface.
  @param[in] using_net_ns Whether the caller is running inside a configured
                          network namespace. When true, interfaces in the probe
                          are not filtered by active state.
  @param[in] candidate_host_or_ip The host or IP part of the candidate
                                  endpoint being checked. It may be either a
                                  hostname or an IP address.
  @param[in] candidate_port The port portion of the candidate endpoint being
                            checked.

  @retval true The candidate endpoint matches the configured XCom identity and
               the matched address resolves to a valid local interface in the
               current network context.
  @retval false The candidate endpoint does not match the configured identity,
                the configured identity cannot be resolved or validated, or the
                matched address does not correspond to a valid local interface
                in the current network context.
*/
static bool is_candidate_endpoint_configured_self(
    sock_probe *s, bool using_net_ns, char const *candidate_host_or_ip,
    xcom_port candidate_port) {
  // Step 1: Reject malformed candidates and missing configured identities
  // before doing any address resolution.
  if (candidate_host_or_ip == nullptr) return false;

  if (!is_xcom_identity_configured()) return false;

  node_address *identity{cfg_app_xcom_get_identity()};

  char configured_host_or_ip[IP_MAX_SIZE];
  xcom_port configured_port = 0;
  // Step 2: Split the configured identity endpoint into host/IP and port.
  // If parsing fails, the configured identity cannot be compared against the
  // candidate. The same guard also rejects candidates whose port already
  // differs from the configured identity port, because in either case the
  // candidate cannot be the configured self endpoint.
  if (get_ip_and_port(identity->address, configured_host_or_ip,
                      &configured_port) ||
      configured_port != candidate_port) {
    return false;
  }

  struct addrinfo *configured_addr = nullptr;
  struct addrinfo *configured_addr_head = nullptr;
  struct addrinfo *candidate_addr = nullptr;
  struct addrinfo *candidate_addr_head = nullptr;
  Xcom_scope_guard cleanup_guard([&]() {
    if (configured_addr_head != nullptr)
      probe_free_addrinfo(configured_addr_head);
    if (candidate_addr_head != nullptr)
      probe_free_addrinfo(candidate_addr_head);
  });

  // Step 3: Resolve the configured identity to all of its socket-address
  // aliases. If that fails, the configured endpoint cannot be validated as
  // self.
  configured_addr_head = configured_addr =
      probe_get_addrinfo(configured_host_or_ip);
  if (configured_addr_head == nullptr) return false;

  // Step 4: Fast-path exact textual matches. When the configured host/IP text
  // is identical to the candidate host/IP text and the port already matched,
  // only the local-interface validation remains.
  if (std::string_view{configured_host_or_ip} == candidate_host_or_ip) {
    return has_address_on_valid_local_interface(s, configured_addr_head,
                                                using_net_ns);
  }

  // Step 5: Resolve the candidate host/IP to all of its aliases so the
  // comparison can succeed across equivalent hostnames and IP addresses.
  candidate_addr_head = candidate_addr =
      probe_get_addrinfo(candidate_host_or_ip);

  if (candidate_addr == nullptr) return false;

  // Step 6: Compare every configured-identity alias with every candidate
  // alias. A shared resolved address is only accepted if that configured
  // identity address is present on a valid local interface in the current
  // network context.
  while (configured_addr != nullptr) {
    candidate_addr = candidate_addr_head;
    while (candidate_addr != nullptr) {
      if (sockaddr_default_eq(configured_addr->ai_addr,
                              candidate_addr->ai_addr) &&
          is_address_on_valid_local_interface(s, configured_addr->ai_addr,
                                              using_net_ns)) {
        return true;
      }
      candidate_addr = candidate_addr->ai_next;
    }
    configured_addr = configured_addr->ai_next;
  }

  return false;
}

node_no xcom_find_node_index(node_list *nodes) {
  node_no i;
  char name[IP_MAX_SIZE];
  xcom_port port = 0;
  std::string net_namespace;
  const auto use_configured_identity{is_xcom_identity_configured()};
  bool using_net_ns = false;

  sock_probe *s = nullptr;

  // Without a port-matching predicate there is no valid way to classify any
  // candidate endpoint as local.
  if (match_port == nullptr) return VOID_NODE_NO;

  Network_namespace_manager *ns_mgr = cfg_app_get_network_namespace_manager();
  if (ns_mgr) ns_mgr->channel_get_network_namespace(net_namespace);
  if (!net_namespace.empty()) {  // If the namespace is configured
                                 /* purecov: begin deadcode */
    ns_mgr->set_network_namespace(net_namespace);
    /* purecov: end */
  }
  using_net_ns = !net_namespace.empty();
  Xcom_scope_guard cleanup_guard([&]() {
    if (!net_namespace.empty()) {  // If the namespace is configured
      ns_mgr->restore_original_network_namespace();
    }

    if (s != nullptr) close_sock_probe(s);
  });

  s = (sock_probe *)xcom_calloc((size_t)1, sizeof(sock_probe));
  if (init_sock_probe(s) < 0) {
    return VOID_NODE_NO;
  }

  /* For each node in list */
  for (i = 0; i < nodes->node_list_len; i++) {
    /* Get host name from host:port string */
    if (get_ip_and_port(nodes->node_list_val[i].address, name, &port)) {
      G_DEBUG("Error parsing IP and Port. Passing to the next node.");
      continue;
    }

    /* See if port matches first */
    if (!match_port(port)) {
      continue;
    }

    if (use_configured_identity) {
      if (is_candidate_endpoint_configured_self(s, using_net_ns, name, port)) {
        return i;
      }
      continue;
    }

    /* Get addresses of host */
    struct addrinfo *addr = probe_get_addrinfo(name);
    XCOM_IFDBG(D_NONE, FN; STRLIT("name "); STRLIT(name); PTREXP(addr));
    Xcom_scope_guard addr_cleanup_guard([&]() {
      if (addr != nullptr) probe_free_addrinfo(addr);
    });

    if (addr != nullptr &&
        has_address_on_valid_local_interface(s, addr, using_net_ns)) {
      return i;
    }
  }

  return VOID_NODE_NO;
}

node_no xcom_mynode_match(const char *name, xcom_port port) {
  std::string net_namespace;
  const auto use_configured_identity{is_xcom_identity_configured()};
  sock_probe *s = nullptr;
  Network_namespace_manager *ns_mgr = cfg_app_get_network_namespace_manager();

  if (match_port && !match_port(port)) return 0;

  if (ns_mgr) ns_mgr->channel_get_network_namespace(net_namespace);
  if (!net_namespace.empty()) {  // If the namespace is configured
                                 /* purecov: begin deadcode */
    ns_mgr->set_network_namespace(net_namespace);
    /* purecov: end */
  }
  bool const using_net_ns = !net_namespace.empty();
  Xcom_scope_guard cleanup_guard([&]() {
    if (!net_namespace.empty()) {  // If the namespace is configured
      ns_mgr->restore_original_network_namespace();
    }

    if (s != nullptr) close_sock_probe(s);
  });

  s = (sock_probe *)xcom_calloc((size_t)1, sizeof(sock_probe));

  if (init_sock_probe(s) < 0) {
    return 0;
  }

  if (use_configured_identity) {
    // xcom_mynode_match returns 1 for match and 0 for mismatch, so the
    // boolean helper result is returned through the implicit bool-to-node_no
    // conversion.
    return is_candidate_endpoint_configured_self(s, using_net_ns, name, port);
  }

  struct addrinfo *addr = probe_get_addrinfo(name);
  XCOM_IFDBG(D_NONE, FN; STREXP(name); PTREXP(addr));
  Xcom_scope_guard addr_cleanup_guard([&]() {
    if (addr != nullptr) probe_free_addrinfo(addr);
  });

  if (addr != nullptr &&
      has_address_on_valid_local_interface(s, addr, using_net_ns)) {
    return 1;
  }

  return 0;
}
