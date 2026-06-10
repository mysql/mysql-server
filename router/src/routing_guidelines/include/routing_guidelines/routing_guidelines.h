/*
 * Copyright (c) 2024, 2026, Oracle and/or its affiliates.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0,
 * as published by the Free Software Foundation.
 *
 * This program is designed to work with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms,
 * as designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an additional
 * permission to link the program and your derivative works with the
 * separately licensed software that they have either included with
 * the program or referenced in the documentation.
 *
 * This program is distributed in the hope that it will be useful,  but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See
 * the GNU General Public License, version 2.0, for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef ROUTER_SRC_ROUTING_GUIDELINES_INCLUDE_ROUTING_GUIDELINES_ROUTING_GUIDELINES_H_
#define ROUTER_SRC_ROUTING_GUIDELINES_INCLUDE_ROUTING_GUIDELINES_ROUTING_GUIDELINES_H_

#include <forward_list>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

#ifdef RAPIDJSON_NO_SIZETYPEDEFINE
#include "my_rapidjson_size_t.h"
#endif

#include <rapidjson/document.h>

#include "my_compiler.h"
#include "mysql/harness/net_ts/internet.h"
#include "mysqlrouter/routing_guidelines_export.h"

namespace routing_guidelines {

namespace rpn {
class Expression;
}  // namespace rpn

/// Keyword meaning that the member/cluster role is undefined.
constexpr char kUndefinedRole[] = "UNDEFINED";

/** Information about this Router instance. */
struct ROUTING_GUIDELINES_EXPORT Router_info {
  // std::less<> is used to allow heterogeneous lookup since key type is
  // std::string
  using tags_t = std::map<std::string, std::string, std::less<>>;

  // port numbers configured for the named port configuration
  uint16_t port_ro{0};
  uint16_t port_rw{0};
  uint16_t port_rw_split{0};

  std::string local_cluster;  //!< name of the local cluster
  std::string hostname;       //!< hostname where router is running
  std::string bind_address;   //!< address on which router is listening
  tags_t tags;  //!< an object containing user defined tags stored in the
                //!< cluster metadata for that Router instance
  std::string route_name;  //!< name of the plugin which handles the
                           //!< connection
  std::string name;        //!< name of the Router instance
};

/** Information about one server destination. */
struct ROUTING_GUIDELINES_EXPORT Server_info {
  std::string label;        //!< hostname:port as in the metadata
  std::string address;      //!< address of the server
  uint16_t port{0};         //!< MySQL port number
  uint16_t port_x{0};       //!< X protocol port number
  std::string uuid;         //!< @@server_uuid of the server
  uint32_t version{0};      //!< server version in format 80401 for 8.4.1
  std::string member_role;  //!< PRIMARY, SECONDARY or READ_REPLICA, as reported
                            //!< by GR, empty string if not defined
  std::map<std::string, std::string, std::less<>>
      tags;                  //!< user defined tags stored in the cluster
                             //!< metadata for that Server instance
  std::string cluster_name;  //!< name of the cluster the server belongs to
  std::string
      cluster_set_name;      //!< name of the ClusterSet the server belongs to
  std::string cluster_role;  //!< PRIMARY or REPLICA depending on the role of
                             //!< the cluster in the ClusterSet, empty string if
                             //!< not defined
  bool cluster_is_invalidated{false};  //!< Cluster containing this server is
                                       //!< invalidated
};

/** Information about incoming session. */
struct ROUTING_GUIDELINES_EXPORT Session_info {
  std::string target_ip;  //!< address of the Router the session is connected to
  int target_port{0};     //!< Router port the session is connected to
  std::string source_ip;  //!< IP address the session is connecting from
  std::string user;       //!< username the session is authenticated with
  std::map<std::string, std::string, std::less<>>
      connect_attrs;   //!< session connect attributes by name
  std::string schema;  //!< default schema specified at connect time
  uint64_t id{0};      //!< an auto-incremented integer number assigned by the
                       //!< router to each session
  double random_value{0};  //!< random value in a range [0-1)
};

/** Information about query details. */
struct ROUTING_GUIDELINES_EXPORT Sql_info {
  std::string default_schema;  //!< schema currently active for the session
  bool is_read{true};          //!< statement (or transaction) is a RO statement
  bool is_update{false};       //!< statement (or transaction) is an update
  bool is_ddl{false};          //!< statement is a DDL operation
  std::map<std::string, std::string, std::less<>>
      query_tags;  //!< query specific tags specified as a comment in the SQL
                   //!< statement ( e.g.: /*-> tag1=value2,tag2=value2 */ )
  std::map<std::string, std::string, std::less<>>
      query_hints;  //!< query specific hints specified at
                    //!< the protocol level (see WL#12542)
};

// The following warning:
// C4275: non dll-interface class 'std::runtime_error' used as base for
// dll-interface class 'routing_guidelines::Guidelines_parse_error'
//
// Can be suppressed, since Visual Studio documentation states that:
//
// "C4275 can be ignored in Visual C++ if you are deriving from a type in the
// C++ Standard Library"
MY_COMPILER_DIAGNOSTIC_PUSH()
MY_COMPILER_MSVC_DIAGNOSTIC_IGNORE(4275)
class ROUTING_GUIDELINES_EXPORT Guidelines_parse_error
    : public std::runtime_error {
 public:
  explicit Guidelines_parse_error(const std::vector<std::string> &errors);

  const std::vector<std::string> &get_errors() const;

 private:
  std::vector<std::string> errors_;
};
MY_COMPILER_DIAGNOSTIC_POP()

/** Information about hostnames that needs to be resolved. */
struct Resolve_host {
  enum class IP_version { IPv4, IPv6 };

  Resolve_host(std::string address_, IP_version ip_version_)
      : address(std::move(address_)), ip_version{ip_version_} {}

  auto operator<=>(const Resolve_host &) const = default;

  std::string address;
  IP_version ip_version;
};

/**
 * Routing guidelines engine.
 *
 * Responsible for traffic classification based on routing guidelines document,
 * information about the given Router instance, incoming session information and
 * destination servers that are provided by the metadata.
 */
class ROUTING_GUIDELINES_EXPORT Routing_guidelines_engine final {
 public:
  Routing_guidelines_engine();
  ~Routing_guidelines_engine();

  Routing_guidelines_engine(Routing_guidelines_engine &&rp);
  Routing_guidelines_engine &operator=(Routing_guidelines_engine &&);

  Routing_guidelines_engine(const Routing_guidelines_engine &) = delete;
  Routing_guidelines_engine &operator=(const Routing_guidelines_engine &) =
      delete;

  /** Get routing guidelines schema describing guidelines document. */
  static std::string get_schema();

  /** Map with preprocessed resolved hostnames. */
  using ResolveCache = std::unordered_map<std::string, net::ip::address>;

  /** Factory method for creating instance of Routing_guidelines_engine
   *
   * @param routing_guidelines_document document content.
   *
   * @returns instance of the Routing_guidelines_engine class.
   *
   * @throws Guidelines_parse_error containing all encountered errors. */
  static Routing_guidelines_engine create(
      const std::string &routing_guidelines_document);

  /**
   * Class representing routing guidelines route section entry.
   *
   * Each route references destinations that are groupped by the destination
   * class section in the routing guideline:
   * @code
   *   "destinations": [
   *     {
   *       "name": "secondary_dests",
   *       "match": "$.server.memberRole = SECONDARY"
   *     }
   *   ]
   * @endcode
   *
   * This example provides a destination class named "secondary_dests" which
   * matches SECONDARY nodes. Given this route:
   * @code
   *   "routes": [
   *     {
   *       "name": "r1",
   *       "enabled": true,
   *       "match": "$.router.port.ro = 6447",
   *       "connectionSharingAllowed": true,
   *       "destinations": [
   *       {
   *         "classes": ["secondary_dests"],
   *         "strategy" : "round-robin"
   *         "priority": 0
   *       }
   *     }
   *   ]
   * @endcode
   * Route named "r1" uses "secondary_dests" destination class. Each node
   * classified in the "secondary_dests" will be used according to the
   * 'round-robin' routing strategy.
   *
   * If one route entry uses multiple destination classes then nodes from each
   * destination classes are used. For example:
   * @code
   *       "destinations": [
   *       {
   *         "classes": ["secondary_dests", "other_dests"],
   *         "strategy" : "round-robin"
   *         "priority": 0
   *       }
   * @endcode
   * Such route will use destinations classified as "secondary_dests" as well as
   * "other_dests".
   *
   * One route may define multiple backup sinks which are used when no
   * destinations from previous groups can be reached. They are groupped by the
   * 'priority' setting, where lower value means higher priority, '0' means
   * highest priority.
   *
   * For example:
   * @code
   * "routes": [
   *   {
   *     "name": "r1",
   *     "enabled": true,
   *     "match": "$.router.port.ro = 6447",
   *     "connectionSharingAllowed": true,
   *     "destinations": [
   *       {
   *         "classes": ["d1", "d2"],
   *         "strategy" : "round-robin"
   *         "priority": 0
   *       },
   *       {
   *         "classes": ["d3"],
   *         "strategy" : "round-robin"
   *         "priority": 1
   *       }
   *     ]
   *   }
   * @endcode
   * Route "r1" defines two destination groups that could be used, one
   * containing destinations classified by "d1" or "d2" destination classes, and
   * the other one containing destinations classified by "d3" destination class.
   * Destinations classified by "d3" will be used if and only if no destination
   * from "d1" and "d2" are reachable.
   */
  struct Route {
    struct DestinationGroup {
      DestinationGroup() = default;
      DestinationGroup(std::vector<std::string> destination_classes_,
                       std::string routing_strategy_, const uint64_t priority_)
          : destination_classes(std::move(destination_classes_)),
            routing_strategy(std::move(routing_strategy_)),
            priority(priority_) {}

      auto operator<=>(const DestinationGroup &) const = default;

      /** References to destinations classified at specific classes. */
      std::vector<std::string> destination_classes;
      /** Routing strategy used to select specific destinations within this
       * group. */
      std::string routing_strategy;
      /** Priority of the group. */
      uint64_t priority{0};
    };

    Route(std::string name, std::unique_ptr<rpn::Expression> match,
          std::vector<DestinationGroup> destination_groups,
          const std::optional<bool> connection_sharing_allowed = std::nullopt,
          const bool enabled = true);

    auto operator<=>(const Route &) const = default;

    /** Name of the route. */
    std::string name;
    /** Matching criterion for the given route. */
    std::unique_ptr<rpn::Expression> match;
    /** Destination groups used by the route. */
    std::vector<DestinationGroup> destination_groups;
    /** Connection sharing enabled flag. */
    std::optional<bool> connection_sharing_allowed;
    /** Route enabled flag. */
    bool enabled{true};
  };

  /**
   * Type for names of Routes changed during routing guidelines document update.
   */
  struct RouteChanges {
    std::string guideline_name;
    std::vector<std::string> affected_routes;
  };

  /**
   * Result of route classification.
   */
  struct Route_classification {
    std::string route_name;
    std::vector<Route::DestinationGroup> destination_groups;
    std::optional<bool> connection_sharing_allowed;
    std::forward_list<std::string> errors;
  };

  /**
   * Result of destination classification.
   */
  struct Destination_classification {
    std::vector<std::string> class_names;
    std::forward_list<std::string> errors;
  };

  /**
   * Update routing guidelines and return affected classes and routes.
   * @param[in] new_rp - new Routing Guidelines engine
   * @param[in] is_provided_by_user - true if guideline is provided by user
   * or false if it is auto-generated.
   */
  RouteChanges update_routing_guidelines(Routing_guidelines_engine &&new_rp,
                                         bool is_provided_by_user = true);

  /** Get routing routing guidelines document name. */
  const std::string &name() const;

  /** Compute a route of a session. */
  Route_classification classify(const Session_info &session,
                                const Router_info &router_info,
                                const Sql_info *sql = nullptr) const;

  /** Compute destination classes to which a MySQL instance belongs.
   *
   * If no suitable class is found class_names vector in returned
   * Destination_classification will be empty. */
  Destination_classification classify(const Server_info &instance,
                                      const Router_info &router_info) const;

  /** Get destination names defined by routing guidelines document. */
  const std::vector<std::string> &destination_classes() const;

  /** Get list of routes defined in routing guidelines. */
  const std::vector<Route> &get_routes() const;

  /** List of hostnames that are used in routing guideline document that need to
   * be resolved. */
  std::vector<Resolve_host> hostnames_to_resolve() const;

  /**
   * Set the resolved hostnames cache, used when hostnames used by the routing
   * guidelines are resolved.
   *
   * Can be called from a different thread, than the one performing
   * classification.
   */
  void update_resolve_cache(ResolveCache cache);

  /**
   * Validate route entry string.
   *
   * @throw std::runtime_error when route entry is invalid
   */
  static void validate_one_route(const std::string &route);

  /**
   * Validate destination entry string.
   *
   * @throw std::runtime_error when destination entry is invalid
   */
  static void validate_one_destination(const std::string &destination);

  /**
   * Validate whole guidelines document.
   *
   * @throw std::runtime_error when guidelines doc is invalid
   */
  static void validate_guideline_document(const std::string &doc);

  /** Get routing guidelines document that is used by the guidelines engine. */
  const rapidjson::Document &get_routing_guidelines_document() const;

  /** Check if routing guideline in use uses extended session info that needs
   * traffic inspection. */
  bool extended_session_info_in_use() const;

  /** Check if routing guideline in use uses random value generated per session.
   */
  bool session_rand_used() const;

  /** Check if the routing guidelines were updated. That means that there is a
   * custom routing guideline in use instead of an auto-generated one.*/
  bool routing_guidelines_updated() const;

  /**
   * Restore auto-generated guideline (based on Router's configuration).
   *
   * @return names of guidelines routes that were affected by this update.
   */
  Routing_guidelines_engine::RouteChanges restore_default();

  /**
   * Set the default guideline (auto-generated based on Router's
   * configuration).
   *
   * @param routing_guidelines_doc routing guidelines document
   */
  void set_default_routing_guidelines(std::string routing_guidelines_doc) {
    default_routing_guidelines_doc_ = std::move(routing_guidelines_doc);
  }

 private:
  /// Compute changes introduced by the new routing guidelines.
  RouteChanges compare(const Routing_guidelines_engine &new_routing_guidelines);

  struct Rpd;
  std::unique_ptr<Rpd> rpd_;
  rapidjson::Document routing_guidelines_document_;
  std::string default_routing_guidelines_doc_;

  friend class Routing_guidelines_document_parser;
};

}  // namespace routing_guidelines

#endif  // ROUTER_SRC_ROUTING_GUIDELINES_INCLUDE_ROUTING_GUIDELINES_ROUTING_GUIDELINES_H_
