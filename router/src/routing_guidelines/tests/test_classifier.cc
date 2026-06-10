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

#include <gmock/gmock-matchers.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <fstream>
#include <iostream>

#ifdef _WIN32
#include <WinSock2.h>
#include <windows.h>
#endif

#include "helpers/router_test_helpers.h"  // EXPECT_THROW_LIKE
#include "mysql/harness/filesystem.h"
#include "routing_guidelines/routing_guidelines.h"
#include "routing_simulator.h"
#include "rules_parser.h"
#include "utils.h"

using mysql_harness::Path;
Path g_here;

namespace routing_guidelines {

TEST(Routing_guidelines_test, incomplete_document) {
  EXPECT_THROW_LIKE(Routing_guidelines_engine::create(""),
                    Guidelines_parse_error, "The document is empty");
  EXPECT_THROW_LIKE(Routing_guidelines_engine::create("{}"),
                    Guidelines_parse_error,
                    "Errors while parsing routing guidelines document");
  EXPECT_THROW_LIKE(
      Routing_guidelines_engine::create("[]"), Guidelines_parse_error,
      "routing guidelines needs to be specified as a JSON document");
  EXPECT_THROW_LIKE(Routing_guidelines_engine::create("{\"routes\": 3}"),
                    Guidelines_parse_error,
                    "routes: field is expected to be an array");
  EXPECT_THROW_LIKE(Routing_guidelines_engine::create(R"({
"name": 1,
"version" : "1.0",
"destinations": [
  {
    "klass": "primary",
    "match": "$.server.role = PRIMARY"
  },
  {
    "name": "secondary",
    "match": ""
  },
  {
    "name": "",
    "match": 5
  }],
"routes":
  [
    {
    "name": "rw",
    "match": "$.session.targetPort = $.router.port.rw",
    "destinations": {"classes": ["primary"], "strategy": "round-robin", "priority": 0}
    },
    {
    "name": "3",
    "match": 3,
    "destination": [{"classes" : ["secondary"], "strategy": "round-robin", "priority": 0},
                    {"classes": ["primary"], "strategy": "first-available", "priority": 1}]
    },
    {
    "name": "ro",
    "match": "$.session.targetPort = $.router.port.ro and $.server.targetPort = $.router.port.ro",
    "destinations": [{"classes": [], "strategy": "roundrobin", "priority": 0},{"classes": ["primary"]}]
    }
  ]})"),
                    Guidelines_parse_error,
                    R"(Errors while parsing routing guidelines document:
- Routing guidelines JSON document schema validation failed: {"type":{"expected":["string"],"actual":"integer","errorCode":20,"instanceRef":"#/name","schemaRef":"#/properties/name"}}
- name: field is expected to be a string
- destinations[0].klass: unexpected field name, only 'name' and 'match' are allowed
- destinations[0].match: undefined variable: server.role in '$.server.role'
- destinations[0]: 'name' field not defined
- destinations[0]: 'match' field not defined
- destinations[1].match: field is expected to be a non empty string
- destinations[1]: 'match' field not defined
- destinations[2].name: field is expected to be a non empty string
- destinations[2].match: field is expected to be a string
- destinations[2]: 'name' field not defined
- destinations[2]: 'match' field not defined
- routes[0].destinations: field is expected to be an array
- routes[1].match: field is expected to be a string
- routes[1].destination: unexpected field, only 'name', 'connectionSharingAllowed', 'enabled', 'match' and 'destinations' are allowed
- routes[1]: 'destinations' field not defined
- routes[2].match: undefined variable: server.targetPort in '$.server.targetPort'
- routes[2].destinations[0].classes: field is expected to be a non empty array
- routes[2].destinations[0].strategy: unexpected value 'roundrobin', supported strategies: round-robin, first-available
- routes[2].destinations[1]: 'strategy' field not defined
- no destination classes defined by the document
- no routes defined by the document)");

  EXPECT_THROW_LIKE(Routing_guidelines_engine::create(R"({
"version" : "1.0",
"destinations": [
  {
    "name": "primary",
    "match": "true"
  },
  {
    "name": "wc",
    "match": "$.server.clusterRole = SECONDARY"
  },
  {
    "name": "wm",
    "match": "$.server.memberRole = REPLICA"
  }],
"routes": [
  {
    "name": "rw",
    "match": "true",
    "destinations": [{"classes": ["primary"], "strategy": "first-available", "priority": 0}]
  }]})"),
                    Guidelines_parse_error,
                    R"(Errors while parsing routing guidelines document:
- destinations[1].match: type error, incompatible operands for comparison: 'CLUSTER ROLE' vs 'MEMBER ROLE' in '$.server.clusterRole = SECONDARY'
- destinations[1]: 'match' field not defined)");
}

namespace {
const char *current_rpd = R"({
"name": "Current router guidelines",
"version" : "1.0",
"destinations": [
  {
    "name": "primary",
    "match": "$.server.memberRole = PRIMARY"
  },
  {
    "name": "secondary",
    "match": "$.server.memberRole = SECONDARY"
  }],
"routes": [
  {
    "name": "rw",
    "match": "$.session.targetPort = $.router.port.rw",
    "destinations": [{"classes": ["primary"],
                      "strategy": "first-available", "priority": 0}]
  },
  {
    "name": "ro",
    "match": "$.session.targetPort = $.router.port.ro",
    "destinations": [{"classes": ["secondary"],
                      "strategy": "round-robin", "priority": 0},
                     {"classes": ["primary"],
                      "strategy": "round-robin", "priority": 1}]
  }
]
})";

const char *cs_rpd = R"^({
  "name": "Cluster sets",
  "version": "1.0",
  "destinations":[
    {
        "name":"serverB",
        "match":"$.server.address = '192.168.5.5'"
    },
    {
        "name":"globalPrimary",
        "match":"$.server.memberRole = PRIMARY and $.server.clusterRole = PRIMARY"
    },
    {
        "name":"otherPrimary",
        "match":"$.server.memberRole = PRIMARY and $.server.clusterRole <> PRIMARY"
    },
    {
        "name":"localSecondaries",
        "match":"$.server.memberRole = SECONDARY and network($.server.address, 24) = network($.router.bindAddress, 24)"
    },
    {
        "name":"remoteSecondaries",
        "match":"$.server.memberRole = SECONDARY and network($.server.address, 24) <> network($.router.bindAddress, 24)"
    }
  ],
  "routes":[
    {
      "name": "192.168.1.13",
      "match":"$.session.sourceIP = '192.168.1.13'",
      "destinations":  [{"classes": ["serverB"],
                         "strategy": "first-available", "priority": 0},
                        {"classes": ["globalPrimary"],
                         "strategy": "first-available", "priority": 1}]
    },
    {
      "name": "app_sync",
      "match":"$.session.user = 'app_sync'",
      "destinations": [{"classes": ["localSecondaries"],
                        "strategy": "round-robin", "priority": 0},
                       {"classes": ["otherPrimary"],
                        "strategy": "first-available", "priority": 1}]
    },
    {
      "name": "reads",
      "match":"$.session.targetPort in ($.router.port.ro)",
      "destinations": [{"classes": ["localSecondaries", "remoteSecondaries"],
                        "strategy": "round-robin", "priority": 0},
                       {"classes": ["globalPrimary"],
                        "strategy": "round-robin", "priority": 1},
                       {"classes": ["serverB"],
                        "strategy": "round-robin", "priority": 2}]
    },
    {
      "name": "writes",
      "match":"$.session.targetPort in ($.router.port.rw)",
      "destinations": [{"classes": ["globalPrimary"],
                        "strategy": "first-available", "priority": 0}]
    }
  ]
}
)^";
}  // namespace

TEST(Routing_guidelines_test, correct_documents) {
  const auto EXPECT_DOC =
      [](const std::string &document, const std::string &name,
         const std::vector<std::string> &dests,
         const std::vector<std::unique_ptr<Routing_guidelines_engine::Route>>
             &routes) {
        SCOPED_TRACE(document);
        auto guidelines = Routing_guidelines_engine::create(document);
        EXPECT_EQ(name, guidelines.name());
        EXPECT_EQ(dests, guidelines.destination_classes());
        const auto &guidelines_routes = guidelines.get_routes();
        EXPECT_EQ(routes.size(), guidelines_routes.size());
        for (const auto &r : routes) {
          EXPECT_THAT(guidelines_routes,
                      ::testing::Contains(::testing::Field(
                          &Routing_guidelines_engine::Route::name, r->name)));
          EXPECT_THAT(
              guidelines_routes,
              ::testing::Contains(::testing::Field(
                  &Routing_guidelines_engine::Route::enabled, r->enabled)));
          EXPECT_THAT(guidelines_routes,
                      ::testing::Contains(::testing::Field(
                          &Routing_guidelines_engine::Route::destination_groups,
                          r->destination_groups)));
        }
      };

  const auto create_match = [](const std::string &match_str) {
    routing_guidelines::Rules_parser parser;
    rpn::Context context;
    context.set("session.targetPort", rpn::Token());
    context.set("router.port.rw", rpn::Token());
    context.set("router.port.ro", rpn::Token());
    context.set("session.sourceIP", rpn::Token());
    context.set("session.user", rpn::Token());
    return std::make_unique<rpn::Expression>(parser.parse(match_str, &context));
  };

  {
    std::vector<std::unique_ptr<Routing_guidelines_engine::Route>> routes;
    routes.emplace_back(std::make_unique<Routing_guidelines_engine::Route>(
        "rw", create_match("$.session.targetPort = $.router.port.rw"),
        std::vector<Routing_guidelines_engine::Route::DestinationGroup>{
            {{"primary"}, "first-available", /*priority*/ 0}}));
    routes.emplace_back(std::make_unique<Routing_guidelines_engine::Route>(
        "ro", create_match("$.session.targetPort = $.router.port.ro"),
        std::vector<Routing_guidelines_engine::Route::DestinationGroup>{
            {{"secondary"}, "round-robin", /*priority*/ 0},
            {{"primary"}, "round-robin", /*priority*/ 1}}));
    EXPECT_DOC(current_rpd, "Current router guidelines",
               {"primary", "secondary"}, routes);
  }

  {
    std::vector<std::unique_ptr<Routing_guidelines_engine::Route>> routes;
    routes.emplace_back(std::make_unique<Routing_guidelines_engine::Route>(
        "192.168.1.13", create_match("$.session.sourceIP = '192.168.1.13'"),
        std::vector<Routing_guidelines_engine::Route::DestinationGroup>{
            {{"serverB"}, "first-available", /*priority*/ 0},
            {{"globalPrimary"}, "first-available", /*priority*/ 1}}));
    routes.emplace_back(std::make_unique<Routing_guidelines_engine::Route>(
        "app_sync", create_match("$.session.user = 'app_sync'"),
        std::vector<Routing_guidelines_engine::Route::DestinationGroup>{
            {{"localSecondaries"}, "round-robin", /*priority*/ 0},
            {{"otherPrimary"}, "first-available", /*priority*/ 1}}));
    routes.emplace_back(std::make_unique<Routing_guidelines_engine::Route>(
        "reads", create_match("$.session.targetPort in ($.router.port.ro)"),
        std::vector<Routing_guidelines_engine::Route::DestinationGroup>{
            {{"localSecondaries", "remoteSecondaries"},
             "round-robin",
             /*priority*/ 0},
            {{"globalPrimary"}, "round-robin", /*priority*/ 1},
            {{"serverB"}, "round-robin", /*priority*/ 2}}));
    routes.emplace_back(std::make_unique<Routing_guidelines_engine::Route>(
        "writes", create_match("$.session.targetPort in ($.router.port.rw)"),
        std::vector<Routing_guidelines_engine::Route::DestinationGroup>{
            {{"globalPrimary"}, "first-available", /*priority*/ 0}}));
    EXPECT_DOC(cs_rpd, "Cluster sets",
               {"serverB", "globalPrimary", "otherPrimary", "localSecondaries",
                "remoteSecondaries"},
               routes);
  }
}

TEST(Routing_guidelines_test, simple_classification) {
  auto rpd = Routing_guidelines_engine::create(current_rpd);
  Router_info router_info{3306,
                          3307,
                          3308,
                          "Cluster",
                          "mysql.oracle.com",
                          "192.168.0.123",
                          {},
                          "routing_plugin_1",
                          "test-router"};
  Server_info server{"NumberOne",
                     "127.0.0.1",
                     3306,
                     33060,
                     "123e4567-e89b-12d3-a456-426614174000",
                     80023,
                     "PRIMARY",
                     {},
                     "",
                     "",
                     kUndefinedRole,
                     false};
  EXPECT_EQ(std::vector<std::string>{"primary"},
            rpd.classify(server, router_info).class_names);
  server.member_role = "SECONDARY";
  EXPECT_EQ(std::vector<std::string>{"secondary"},
            rpd.classify(server, router_info).class_names);
  server.member_role = kUndefinedRole;
  EXPECT_EQ(std::vector<std::string>{},
            rpd.classify(server, router_info).class_names);

  Session_info session{"196.0.0.1", 3306, "123.222.111.12", "root", {},
                       "test",      1};
  EXPECT_EQ("ro", rpd.classify(session, router_info).route_name);
  session.target_port = 3307;

  EXPECT_EQ("rw", rpd.classify(session, router_info).route_name);
  session.target_port = 33071;
  EXPECT_EQ("", rpd.classify(session, router_info).route_name);
}

TEST(Routing_guidelines_test, cs_classification) {
  auto rpd = Routing_guidelines_engine::create(cs_rpd);
  Router_info router_info{3306,
                          3307,
                          3308,
                          "Cluster",
                          "mysql.oracle.com",
                          "192.168.0.123",
                          {},
                          "routing_plugin_1",
                          "test-router"};
  Server_info server{"NumberOne",
                     "192.168.5.5",
                     3306,
                     33060,
                     "123e4567-e89b-12d3-a456-426614174000",
                     80023,
                     "PRIMARY",
                     {},
                     "",
                     "",
                     "READ_REPLICA",
                     false};
  EXPECT_EQ(std::vector<std::string>({"serverB", "otherPrimary"}),
            rpd.classify(server, router_info).class_names);
  server.address = "192.168.5.4";
  EXPECT_EQ(std::vector<std::string>{"otherPrimary"},
            rpd.classify(server, router_info).class_names);
  server.cluster_role = "PRIMARY";
  EXPECT_EQ(std::vector<std::string>{"globalPrimary"},
            rpd.classify(server, router_info).class_names);
  server.member_role = "SECONDARY";
  EXPECT_EQ(std::vector<std::string>{"remoteSecondaries"},
            rpd.classify(server, router_info).class_names);
  server.address = "192.168.0.12";
  EXPECT_EQ(std::vector<std::string>{"localSecondaries"},
            rpd.classify(server, router_info).class_names);
  server.member_role = kUndefinedRole;
  EXPECT_EQ(std::vector<std::string>{},
            rpd.classify(server, router_info).class_names);

  Session_info session{"192.168.0.123", 3306, "192.168.1.13", "root", {},
                       "test",          1};
  EXPECT_EQ("192.168.1.13", rpd.classify(session, router_info).route_name);
  session.source_ip = "192.168.0.55";
  EXPECT_EQ("reads", rpd.classify(session, router_info).route_name);
  session.target_port = 3307;
  EXPECT_EQ("writes", rpd.classify(session, router_info).route_name);
  session.user = "app_sync";
  EXPECT_EQ("app_sync", rpd.classify(session, router_info).route_name);
}

TEST(Routing_guidelines_test, simulator) {
  auto dir = Path(mysql_harness::get_tests_data_dir(g_here.str()));
  dir.append("simulator");

  for (const auto &file : mysql_harness::Directory(dir)) {
    auto fs = std::ifstream(file.str());
    EXPECT_TRUE(fs);
    if (!fs) {
      FAIL() << "Unable to open file: " << file;
    }

    Routing_simulator simulator;
    std::string json_doc;
    for (std::string line; std::getline(fs, line);) {
      line = routing_guidelines::str_strip(line);
      if (line.empty() || line[0] == '#') continue;
      json_doc += line;
      if (!json_document_complete(json_doc)) continue;

      const auto simulator_res = simulator.process_document(json_doc);
      EXPECT_TRUE(simulator_res) << simulator_res.error();
      json_doc.clear();
    }
  }
}

TEST(Routing_guidelines_test, guidelines_update) {
  auto rpd = Routing_guidelines_engine::create(current_rpd);

  const auto EXPECT_CHANGES = [&rpd](const char *document,
                                     std::vector<std::string> changed_routes) {
    auto new_routing_guidelines = Routing_guidelines_engine::create(document);
    auto changes =
        rpd.update_routing_guidelines(std::move(new_routing_guidelines))
            .affected_routes;
    SCOPED_TRACE(rpd.name());

    EXPECT_THAT(changes, ::testing::ContainerEq(changed_routes));
  };

  EXPECT_CHANGES(R"({
"name": "No changes",
"version" : "1.0",
"destinations": [
  {
    "name": "primary",
    "match": "$.server.memberRole = PRIMARY"
  },
  {
    "name": "secondary",
    "match": "$.server.memberRole = SECONDARY"
  }],
"routes": [
  {
    "name": "rw",
    "match": "$.session.targetPort = $.router.port.rw",
    "destinations": [{"classes": ["primary"],
                      "strategy": "first-available", "priority": 0}]
  },
  {
    "name": "ro",
    "match": "$.session.targetPort = $.router.port.ro",
    "destinations": [{"classes": ["secondary"],
                      "strategy": "round-robin", "priority": 0},
                     {"classes": ["primary"],
                      "strategy": "round-robin", "priority": 1}]
  }
]
})",
                 {});

  EXPECT_CHANGES(R"({
"name": "Destinations just rearanged, rule changed for rw route",
"version" : "1.0",
"destinations": [
  {
    "name": "secondary",
    "match": "$.server.memberRole = SECONDARY"
  },
  {
    "name": "primary",
    "match": "$.server.memberRole = PRIMARY"
  }],
"routes": [
  {
    "name": "rw",
    "match": "$.session.targetPort in ($.router.port.rw) ",
    "destinations": [{"classes": ["primary"],
                      "strategy": "round-robin", "priority": 0}]
  },
  {
    "name": "ro",
    "match": "$.session.targetPort = $.router.port.ro",
    "destinations": [{"classes": ["secondary"],
                      "strategy": "round-robin", "priority": 0},
                     {"classes": ["primary"],
                      "strategy": "round-robin", "priority": 1}]
  }
]
})",
                 {"rw"});

  EXPECT_CHANGES(R"({
"name": "Destination renamed,  all routes renamed",
"version" : "1.0",
"destinations": [
  {
    "name": "secondary",
    "match": "$.server.memberRole = SECONDARY"
  },
  {
    "name": "master",
    "match": "$.server.memberRole = PRIMARY"
  }],
"routes": [
  {
    "name": "writes",
    "match": "$.session.targetPort in ($.router.port.rw) ",
    "destinations": [{"classes": ["master", "secondary"],
                      "strategy": "first-available", "priority": 0}]
  },
  {
    "name": "reads",
    "match": "$.session.targetPort = $.router.port.ro",
    "destinations": [{"classes": ["secondary"],
                      "strategy": "round-robin", "priority": 0},
                     {"classes": ["master"],
                      "strategy": "round-robin", "priority": 1}]
  }
]
})",
                 {"rw", "ro"});

  EXPECT_CHANGES(R"({
"name": "Match expressions changed",
"version" : "1.0",
"destinations": [
  {
    "name": "secondary",
    "match": "NOT $.server.memberRole = PRIMARY"
  },
  {
    "name": "master",
    "match": "$.server.memberRole = PRIMARY"
  }],
"routes": [
  {
    "name": "writes",
    "match": "$.session.randomValue < 0.5",
    "destinations": [{"classes": ["master", "secondary"],
                      "strategy": "first-available", "priority": 0}]
  },
  {
    "name": "reads",
    "match": "$.session.randomValue > 0.5",
    "destinations": [{"classes": ["secondary"],
                      "strategy": "round-robin", "priority": 0},
                     {"classes": ["master"],
                      "strategy": "round-robin", "priority": 1}]
  }
]
})",
                 {"writes", "reads"});

  EXPECT_CHANGES(R"({
"name": "Route order changed",
"version" : "1.0",
"destinations": [
  {
    "name": "secondary",
    "match": "NOT $.server.memberRole = PRIMARY"
  },
  {
    "name": "master",
    "match": "$.server.memberRole = PRIMARY"
  }],
"routes": [
  {
    "name": "reads",
    "match": "$.session.randomValue > 0.5",
    "destinations": [{"classes": ["secondary"],
                      "strategy": "round-robin", "priority": 0},
                     {"classes": ["master"],
                      "strategy": "round-robin", "priority": 1}]
  },
  {
    "name": "writes",
    "match": "$.session.randomValue < 0.5",
    "destinations": [{"classes": ["master", "secondary"],
                      "strategy": "first-available", "priority": 0}]
  }
]
})",
                 {});

  EXPECT_CHANGES(R"({
"name": "Priority changed",
"version" : "1.0",
"destinations": [
  {
    "name": "secondary",
    "match": "NOT $.server.memberRole = PRIMARY"
  },
  {
    "name": "master",
    "match": "$.server.memberRole = PRIMARY"
  }],
"routes": [
  {
    "name": "reads",
    "match": "$.session.randomValue > 0.5",
    "destinations": [{"classes": ["secondary"],
                      "strategy": "round-robin", "priority": 0},
                     {"classes": ["master"],
                      "strategy": "round-robin", "priority": 1}]
  },
  {
    "name": "writes",
    "match": "$.session.randomValue < 0.5",
    "destinations": [{"classes": ["master", "secondary"],
                      "strategy": "first-available", "priority": 99}]
  }
]
})",
                 {"writes"});

  EXPECT_CHANGES(R"({
"name": "New route added",
"version" : "1.0",
"destinations": [
  {
    "name": "secondary",
    "match": "NOT $.server.memberRole = PRIMARY"
  },
  {
    "name": "master",
    "match": "$.server.memberRole = PRIMARY"
  },
  {
    "name": "unknown",
    "match": "$.server.memberRole = UNDEFINED"
  }],
"routes": [
  {
    "name": "reads",
    "match": "$.session.randomValue > 0.5",
    "destinations": [{"classes": ["secondary"],
                      "strategy": "round-robin", "priority": 0},
                     {"classes": ["master"],
                      "strategy": "round-robin", "priority": 1}]
  },
  {
    "name": "writes",
    "match": "$.session.randomValue < 0.5",
    "destinations": [{"classes": ["master", "secondary"],
                      "strategy": "first-available", "priority": 99}]
  }
]
})",
                 {});

  EXPECT_CHANGES(R"({
"name": "Route removed",
"version" : "1.0",
"destinations": [
  {
    "name": "secondary",
    "match": "NOT $.server.memberRole = PRIMARY"
  },
  {
    "name": "master",
    "match": "$.server.memberRole = PRIMARY"
  }],
"routes": [
   {
    "name": "reads",
    "match": "$.session.randomValue > 0.5",
    "destinations": [{"classes": ["secondary"],
                      "strategy": "round-robin", "priority": 0},
                     {"classes": ["master"],
                      "strategy": "round-robin", "priority": 1}]
  },
  {
    "name": "writes",
    "match": "$.session.randomValue < 0.5",
    "destinations": [{"classes": ["master", "secondary"],
                      "strategy": "first-available", "priority": 99}]
  }
]
})",
                 {});

  EXPECT_CHANGES(R"({
"name": "Route destinations changed",
"version" : "1.0",
"destinations": [
  {
    "name": "secondary",
    "match": "NOT $.server.memberRole = PRIMARY"
  },
  {
    "name": "master",
    "match": "$.server.memberRole = PRIMARY AND $.server.version >= 90000"
  }],
"routes": [
   {
    "name": "reads",
    "match": "$.session.randomValue > 0.5",
    "destinations": [{"classes": ["secondary"],
                      "strategy": "round-robin", "priority": 0},
                     {"classes": ["master"],
                      "strategy": "round-robin", "priority": 1}]
  },
  {
    "name": "writes",
    "match": "$.session.randomValue < 0.5",
    "destinations": [{"classes": ["master", "secondary"],
                      "strategy": "first-available", "priority": 99}]
  }
]
})",
                 {"reads", "writes"});
}

TEST(Routing_guidelines_test, resolve_caching) {
  const std::string document(R"({
  "name": "Resolve test",
  "version" : "1.0",
  "destinations": [
    {
      "name": "abra",
      "match": "$.server.address = resolve_v4('abra') "
    },
    {
      "name": "cadabra",
      "match": "$.server.address = resolve_v6('cadabra') "
    }],
  "routes": [
    {
      "name": "AB",
      "match": "$.session.sourceIP = resolve_v4(Abra) ",
      "destinations": [{"classes": ["abra"],
                        "strategy": "first-available", "priority": 0}]
    },
    {
      "name": "CD",
      "match": "$.session.sourceIP = resolve_v6(abracadabra) ",
      "destinations": [{"classes": ["cadabra"],
                        "strategy": "round-robin", "priority": 0}]
    }
  ]
}
)");

  auto classifier = Routing_guidelines_engine::create(document);
  EXPECT_THAT(std::vector<Resolve_host>(
                  {{"abra", Resolve_host::IP_version::IPv4},
                   {"cadabra", Resolve_host::IP_version::IPv6},
                   {"abracadabra", Resolve_host::IP_version::IPv6}}),
              ::testing::ContainerEq(classifier.hostnames_to_resolve()));

  Routing_guidelines_engine::ResolveCache cache;
  cache.emplace("abra", net::ip::make_address("123.12.13.11").value());
  cache.emplace("cadabra", net::ip::make_address_v6("::ffff:3.3.3.3").value());
  cache.emplace("abracadabra",
                net::ip::make_address_v6("::ffff:4.4.4.4").value());
  classifier.update_resolve_cache(std::move(cache));

  Server_info server{"NumberOne",
                     "123.12.13.11",
                     3306,
                     33060,
                     "123e4567-e89b-12d3-a456-426614174000",
                     80023,
                     "PRIMARY",
                     {},
                     "",
                     "",
                     "REPLICA",
                     false};
  Router_info router_info;
  EXPECT_EQ(std::vector<std::string>{"abra"},
            classifier.classify(server, router_info).class_names);
  server.address = "::ffff:3.3.3.3";
  EXPECT_EQ(std::vector<std::string>{"cadabra"},
            classifier.classify(server, router_info).class_names);

  Session_info session{"196.0.0.1", 3306, "123.12.13.11", "root", {},
                       "test",      1};
  EXPECT_EQ("AB", classifier.classify(session, router_info).route_name);
  session.source_ip = "::ffff:4.4.4.4";
  EXPECT_EQ("CD", classifier.classify(session, router_info).route_name);

  Routing_guidelines_engine::ResolveCache new_cache;
  new_cache.emplace("abra", net::ip::make_address("3.3.3.3").value());
  new_cache.emplace("cadabra",
                    net::ip::make_address_v6("::ffff:123.12.13.11").value());
  new_cache.emplace("abracadabra",
                    net::ip::make_address_v6("::ffff:5.5.5.5").value());
  classifier.update_resolve_cache(std::move(new_cache));

  server.address = "3.3.3.3";
  EXPECT_EQ(std::vector<std::string>{"abra"},
            classifier.classify(server, router_info).class_names);
  server.address = "::ffff:123.12.13.11";
  EXPECT_EQ(std::vector<std::string>{"cadabra"},
            classifier.classify(server, router_info).class_names);

  session.source_ip = "3.3.3.3";
  EXPECT_EQ("AB", classifier.classify(session, router_info).route_name);
  session.source_ip = "::ffff:5.5.5.5";
  EXPECT_EQ("CD", classifier.classify(session, router_info).route_name);

  // Cache preserved during document update
  const std::string document1(R"({
    "name": "Resolve test",
    "version" : "1.0",
    "destinations": [
      {
        "name": "abra",
        "match": "$.server.address = resolve_v4('abra') "
      },
      {
        "name": "cadabra",
        "match": "$.server.address = resolve_v6('cadabra') "
      }],
    "routes": [
      {
        "name": "AB",
        "match": "$.session.sourceIP = resolve_v4(Abra) ",
        "destinations": [{"classes": ["abra"],
                          "strategy": "first-available", "priority": 0}]
      },
      {
        "name": "EF",
        "match": "$.session.sourceIP = resolve_v6(abracadabra) ",
        "destinations": [{"classes": ["cadabra"],
                          "strategy": "round-robin", "priority": 0}]
      }
    ]
  }
  )");

  auto new_classifier = Routing_guidelines_engine::create(document1);
  classifier.update_routing_guidelines(std::move(new_classifier));

  // No cache - classification will fail
  classifier.update_resolve_cache(Routing_guidelines_engine::ResolveCache());
  EXPECT_EQ(
      std::forward_list<std::string>(
          {"destinations.cadabra: No cache entry to resolve host: cadabra",
           "destinations.abra: No cache entry to resolve host: abra"}),
      classifier.classify(server, router_info).errors);

  EXPECT_EQ(std::forward_list<std::string>(
                {"route.EF: No cache entry to resolve host: abracadabra",
                 "route.AB: No cache entry to resolve host: abra"}),
            classifier.classify(session, router_info).errors);
}

}  // namespace routing_guidelines

int main(int argc, char *argv[]) {
#ifdef _WIN32
  WORD wVersionRequested = MAKEWORD(2, 2);
  WSADATA wsaData;
  if (int err = WSAStartup(wVersionRequested, &wsaData)) {
    std::cerr << "WSAStartup failed with code " << err << std::endl;
    return 1;
  }
#endif

  g_here = Path(argv[0]).dirname();
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
