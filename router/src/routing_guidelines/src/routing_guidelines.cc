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
#include "routing_guidelines/routing_guidelines.h"

#include <algorithm>
#include <cassert>
#include <memory>
#include <optional>
#include <utility>

#ifdef RAPIDJSON_NO_SIZETYPEDEFINE
#include "my_rapidjson_size_t.h"
#endif

#include <rapidjson/document.h>
#include <rapidjson/error/en.h>
#include <rapidjson/schema.h>
#include <rapidjson/writer.h>

#include "guidelines_schema.h"
#include "mysql/harness/stdx/ranges.h"     // enumerate, contains
#include "mysql/harness/utility/string.h"  // mysql_harness::join string_format
#include "mysqlrouter/routing_guidelines_version.h"
#include "rules_parser.h"
#include "utils.h"  // format_json_error

namespace routing_guidelines {

namespace {
std::string format_parse_error(const std::vector<std::string> &errors) {
  assert(!errors.empty());
  return (errors.size() > 1
              ? "Errors while parsing routing guidelines document:\n- "
              : "Error while parsing routing guidelines document:\n- ") +
         mysql_harness::join(errors, "\n- ");
}
}  // namespace

Guidelines_parse_error::Guidelines_parse_error(
    const std::vector<std::string> &errors)
    : std::runtime_error(format_parse_error(errors)), errors_(errors) {}

const std::vector<std::string> &Guidelines_parse_error::get_errors() const {
  return errors_;
}

struct Routing_guidelines_engine::Rpd {
  std::string name;
  std::vector<std::string> dest_names;
  std::vector<rpn::Expression> dest_rules;
  std::vector<Routing_guidelines_engine::Route> routes;
  ResolveCache cache;
  std::vector<Resolve_host> hostnames_to_resolve;
  bool extended_session_info_in_use{false};
  bool session_rand_used{false};
  bool guidelines_updated{false};
};

class Routing_guidelines_document_parser {
 public:
  Routing_guidelines_engine operator()(const std::string &document) {
    rapidjson::Document doc;
    rapidjson::ParseResult ok = doc.Parse(document);
    if (!ok) {
      errors_.emplace_back(format_json_error(document, ok, 15));
    } else if (!doc.IsObject()) {
      errors_.emplace_back(
          "routing guidelines needs to be specified as a JSON document");
    } else {
      rapidjson::Document schema;
      if (schema.Parse(Routing_guidelines_engine::get_schema().c_str())
              .HasParseError()) {
        errors_.emplace_back("Invalid guidelines document schema");
        errors_.emplace_back(GetParseError_En(schema.GetParseError()));
        errors_.emplace_back(Routing_guidelines_engine::get_schema());
      }
      rapidjson::SchemaDocument schema_doc{schema};
      rapidjson::SchemaValidator validator{schema_doc};
      if (!doc.Accept(validator)) {
        const auto &validation_error_info_obj = validator.GetError();
        rapidjson::StringBuffer sb;
        rapidjson::Writer<rapidjson::StringBuffer> writer(sb);
        validation_error_info_obj.Accept(writer);
        const std::string err_msg = sb.GetString();
        add_error(
            "Routing guidelines JSON document schema validation failed: " +
            err_msg);
      }

      if (doc.HasMember("version")) {
        context_.set_version(
            mysqlrouter::routing_guidelines_version_from_string(
                doc["version"].GetString()));
      }

      for (const auto &member : doc.GetObject()) {
        auto d = push_scope(member.name.GetString());
        if (str_eq("version", member.name.GetString())) {
          continue;
        } else if (str_eq("destinations", member.name.GetString())) {
          parse_rules(member.value, &rpd_.dest_names, &rpd_.dest_rules);
        } else if (str_eq("routes", member.name.GetString())) {
          parse_routes(member.value);
        } else if (str_eq("name", member.name.GetString())) {
          if (is_string_value(member.value))
            rpd_.name = member.value.GetString();
        } else {
          add_error(
              "Unexpected field, only 'version', 'name', 'destinations', and "
              "'routes' are allowed");
        }
      }
      if (rpd_.dest_names.empty())
        add_error("no destination classes defined by the document");
      if (rpd_.routes.empty()) add_error("no routes defined by the document");

      // validate names in routes match defined names
      if (errors_.empty()) {
        auto d = push_scope("routes");
        for (const auto &p : rpd_.routes) {
          for (const auto &out : p.destination_groups) {
            for (const auto &in : out.destination_classes) {
              if (!stdx::ranges::contains(rpd_.dest_names, in)) {
                add_error("undefined destination class '" + in + "'" +
                          " found in route '" + p.name + "'");
              }
            }
          }
        }
      }
    }

    if (!errors_.empty()) throw Guidelines_parse_error(errors_);

    routing_guidelines_.routing_guidelines_document_ = std::move(doc);
    return std::move(routing_guidelines_);
  }

  void validate_rule(const rapidjson::Value &rule) {
    parse_rule(rule);
    if (!errors_.empty()) throw Guidelines_parse_error(errors_);
  }

  void validate_route(const rapidjson::Value &route) {
    parse_route(route);
    if (!errors_.empty()) throw Guidelines_parse_error(errors_);
  }

  enum class Match_role_type { Destination, Route };

 private:
  // Used to pop json_scope at the end of a scope
  static void pop_scope(std::vector<std::string> *scope) { scope->pop_back(); }

  using Deleter = std::unique_ptr<std::vector<std::string>,
                                  void (*)(std::vector<std::string> *)>;

  Deleter push_scope(const std::string &s) {
    if (json_scope_.empty())
      json_scope_.emplace_back(s);
    else
      json_scope_.emplace_back("." + s);
    return Deleter(&json_scope_, pop_scope);
  }

  Deleter push_scope(int n) {
    json_scope_.emplace_back("[" + std::to_string(n) + "]");
    return Deleter(&json_scope_, pop_scope);
  }

  void add_error(const std::string &msg) {
    auto scope = mysql_harness::join(json_scope_, "");
    if (!scope.empty()) scope += ": ";
    errors_.emplace_back(scope + msg);
  }

  // We do not expect empty strings for any field
  bool is_string_value(const rapidjson::Value &elem) {
    if (!elem.IsString()) {
      add_error("field is expected to be a string");
      return false;
    } else if (0 == elem.GetStringLength()) {
      add_error("field is expected to be a non empty string");
      return false;
    }
    return true;
  }

  bool is_object_value(const rapidjson::Value &elem) {
    bool ret = elem.IsObject();
    if (!ret) add_error("field is expected to be an object");
    return ret;
  }

  bool is_bool_value(const rapidjson::Value &elem) {
    bool ret = elem.IsBool();
    if (!ret) add_error("field is expected to be boolean");
    return ret;
  }

  bool is_array_value(const rapidjson::Value &elem) {
    if (!elem.IsArray()) {
      add_error("field is expected to be an array");
      return false;
    } else if (elem.GetArray().Empty()) {
      add_error("field is expected to be a non empty array");
      return false;
    }
    return true;
  }

  std::optional<rpn::Expression> parse_matching_rule(
      const std::string &match_str, Match_role_type match_role_type) {
    std::optional<rpn::Expression> match;
    try {
      Session_info session_info;
      context_.set_session_info(session_info);
      Server_info server_info;
      context_.set_server_info(server_info);
      Sql_info sql_info;
      context_.set_sql_info(sql_info);
      Router_info router_info;
      context_.set_router_info(router_info);
      match = parser_.parse(match_str, &context_);
      rpd_.extended_session_info_in_use = parser_.extended_session_info_used();
      rpd_.session_rand_used = parser_.session_rand_used();

      // verify VAR_REF context, find hostnames to resolve, fill temporary cache
      for (const auto &tok : match->rpn_) {
        if (tok.type() == rpn::Token::Type::VAR_REF) {
          const std::string var_name = context_.get_var_name(tok).value_or("");
          switch (match_role_type) {
            case Match_role_type::Destination:
              if (var_name.starts_with("session"))
                add_error(var_name +
                          " may not be used in 'destinations' context");
              break;
            case Match_role_type::Route:
              if (var_name.starts_with("server"))
                add_error(var_name + " may not be used in 'routes' context");
              break;
          }
        }

        if ((tok.type() == rpn::Token::Type::RESOLVE_V4 ||
             tok.type() == rpn::Token::Type::RESOLVE_V6)) {
          const auto type =
              tok.type() == rpn::Token::Type::RESOLVE_V4
                  ? routing_guidelines::Resolve_host::IP_version::IPv4
                  : routing_guidelines::Resolve_host::IP_version::IPv6;
          if (std::find_if(std::begin(rpd_.hostnames_to_resolve),
                           std::end(rpd_.hostnames_to_resolve),
                           [&tok, &type](const auto &host) {
                             return host.address == tok.string() &&
                                    type == host.ip_version;
                           }) == std::end(rpd_.hostnames_to_resolve)) {
            rpd_.hostnames_to_resolve.emplace_back(tok.string(), type);
          }
        }
      }

      if (!match->verify(&context_)) {
        add_error("match does not evaluate to boolean");
        if (match) match->clear();
      }
    } catch (const std::exception &e) {
      add_error(e.what());
      if (match) match->clear();
    }
    return match;
  }

  std::pair<std::optional<std::string>, std::optional<rpn::Expression>>
  parse_rule(const rapidjson::Value &rule) {
    std::optional<std::string> name;
    std::optional<rpn::Expression> match;

    for (const auto &member : rule.GetObject()) {
      auto d = push_scope(member.name.GetString());
      if (str_eq("name", member.name.GetString())) {
        if (is_string_value(member.value)) name = member.value.GetString();
      } else if (str_eq("match", member.name.GetString())) {
        if (!is_string_value(member.value)) continue;
        match = parse_matching_rule(member.value.GetString(),
                                    Match_role_type::Destination);
      } else {
        add_error("unexpected field name, only 'name' and 'match' are allowed");
      }
    }
    return {name, match};
  }

  void parse_rules(const rapidjson::Value &elem,
                   std::vector<std::string> *names,
                   std::vector<rpn::Expression> *rules) {
    if (!is_array_value(elem)) return;

    int index = 0;
    for (const auto &rule : elem.GetArray()) {
      auto di = push_scope(index++);
      if (!is_object_value(rule)) continue;

      auto [name, match] = parse_rule(rule);

      if (!name) add_error("'name' field not defined");
      if (!match) add_error("'match' field not defined");
      if (!name || !match) continue;

      if (!name->empty() && !match->empty()) {
        if (stdx::ranges::contains(*names, *name)) {
          add_error("'" + *name + "' class was already defined");
        } else {
          rules->emplace_back(std::move(*match));
          names->emplace_back(*name);
        }
      }
    }
  }

  //  Routes are defined as an array of objects containing 2 field:
  //  - "match" - string defining source matchin criteria
  //  - "destinations" - array of destination objects conisiting of :
  //    - array of destination classes
  //    - routing strategy name
  //
  //  "routes": [
  //    {
  //      "match": "$.session.targetPort = 6446",
  //      "destinations": [
  //      {
  //        "classes": ["primary"],
  //        "strategy" : "first-available"
  //      }
  //      ]
  //    },
  //    {
  //      "match": "$.session.targetPort = 6447",
  //      "destinations": [
  //      {
  //        "classes": ["secondary"],
  //        "strategy" : "round-robin"
  //      },
  //      {
  //        "classes": ["primary"],
  //        "strategy" : "round-robin"
  //      }
  //      ]
  //    }
  //  ]
  void parse_routes(const rapidjson::Value &elem) {
    if (!is_array_value(elem)) return;

    int index = 0;
    for (const auto &it : elem.GetArray()) {
      auto di = push_scope(index++);

      parse_route(it);
    }
  }

  void parse_route(const rapidjson::Value &elem) {
    if (!is_object_value(elem)) return;

    const auto route_object = elem.GetObject();

    std::string route_name;
    rpn::Expression route_match;
    std::vector<Routing_guidelines_engine::Route::DestinationGroup>
        destination_groups;
    bool match_defined{false}, destinations_defined{false}, name_defined{false};
    bool route_enabled{true};
    std::optional<bool> sharing_allowed;

    for (const auto &cand : elem.GetObject()) {
      auto df = push_scope(cand.name.GetString());
      if (str_eq("destinations", cand.name.GetString())) {
        destinations_defined = true;
        destination_groups = parse_route_destinations(cand.value);
      } else if (str_eq("match", cand.name.GetString())) {
        match_defined = true;
        if (is_string_value(cand.value)) {
          auto route_match_res = parse_matching_rule(cand.value.GetString(),
                                                     Match_role_type::Route);
          if (route_match_res) route_match = route_match_res.value();
        }
      } else if (str_eq("name", cand.name.GetString())) {
        name_defined = true;
        if (is_string_value(cand.value)) route_name = cand.value.GetString();
      } else if (str_eq("enabled", cand.name.GetString())) {
        if (is_bool_value(cand.value)) route_enabled = cand.value.GetBool();
      } else if (str_eq("connectionSharingAllowed", cand.name.GetString())) {
        if (is_bool_value(cand.value)) sharing_allowed = cand.value.GetBool();
      } else {
        add_error(
            "unexpected field, only 'name', 'connectionSharingAllowed', "
            "'enabled', 'match' and 'destinations' are allowed");
      }
    }
    if (!name_defined) add_error("'name' field not defined");
    if (!match_defined) add_error("'match' field not defined");
    if (!destinations_defined) add_error("'destinations' field not defined");
    if (!route_match.empty() && !destination_groups.empty()) {
      if (std::find_if(std::begin(rpd_.routes), std::end(rpd_.routes),
                       [&route_name](const auto &route) {
                         return route.name == route_name;
                       }) != std::end(rpd_.routes)) {
        add_error("'" + route_name + "' route was already defined");
      } else {
        rpd_.routes.emplace_back(
            route_name, std::make_unique<rpn::Expression>(route_match),
            std::move(destination_groups), sharing_allowed, route_enabled);
      }
    }
  }

  std::vector<Routing_guidelines_engine::Route::DestinationGroup>
  parse_route_destinations(const rapidjson::Value &elem) {
    std::vector<Routing_guidelines_engine::Route::DestinationGroup> ret;
    if (is_array_value(elem)) {
      for (auto [index, obj] : stdx::views::enumerate(elem.GetArray())) {
        auto di = push_scope(index++);
        if (!is_object_value(obj)) continue;

        Routing_guidelines_engine::Route::DestinationGroup destination_group;
        bool classes_defined{false}, strategy_defined{false};
        for (const auto &member : obj.GetObject()) {
          auto d = push_scope(member.name.GetString());
          if (str_eq("strategy", member.name.GetString())) {
            strategy_defined = true;
            if (is_string_value(member.value)) {
              std::string strategy = member.value.GetString();
              if (stdx::ranges::contains(k_routing_strategies, strategy)) {
                destination_group.routing_strategy = member.value.GetString();
              } else {
                add_error("unexpected value '" + strategy +
                          "', supported strategies: " +
                          mysql_harness::join(k_routing_strategies, ", "));
              }
            }
          } else if (str_eq("classes", member.name.GetString())) {
            classes_defined = true;
            if (!is_array_value(member.value)) continue;

            for (auto [classes_index, dest_class] :
                 stdx::views::enumerate(member.value.GetArray())) {
              auto dc = push_scope(classes_index++);
              if (is_string_value(dest_class)) {
                destination_group.destination_classes.emplace_back(
                    dest_class.GetString());
              }
            }
          } else if (str_eq("priority", member.name.GetString())) {
            if (member.value.IsUint64()) {
              destination_group.priority = member.value.GetUint64();
            } else {
              add_error("field is expected to be a positive integer");
            }
          } else {
            add_error(
                "unexpected field name, only 'classes' and 'strategy' are "
                "allowed");
          }
        }
        if (!classes_defined) add_error("'classes' field not defined");
        if (!strategy_defined) add_error("'strategy' field not defined");
        if (!destination_group.destination_classes.empty() &&
            !destination_group.routing_strategy.empty()) {
          ret.emplace_back(std::move(destination_group));
        }
      }
    }
    return ret;
  }

  Routing_guidelines_engine routing_guidelines_;
  Routing_guidelines_engine::Rpd &rpd_{*routing_guidelines_.rpd_};
  Rules_parser parser_;
  rpn::Context context_;
  std::vector<std::string> errors_;
  std::vector<std::string> json_scope_;
};

Routing_guidelines_engine Routing_guidelines_engine::create(
    const std::string &document) {
  return Routing_guidelines_document_parser()(document);
}

Routing_guidelines_engine::Routing_guidelines_engine()
    : rpd_(std::make_unique<Rpd>()) {}

// They could not be marked as default in the header file as compiler does not
// know the Rpd size there yet
Routing_guidelines_engine::Routing_guidelines_engine(
    Routing_guidelines_engine &&) = default;

Routing_guidelines_engine &Routing_guidelines_engine::operator=(
    Routing_guidelines_engine &&) = default;

Routing_guidelines_engine::~Routing_guidelines_engine() = default;

Routing_guidelines_engine::RouteChanges Routing_guidelines_engine::compare(
    const Routing_guidelines_engine &new_guidelines) {
  Routing_guidelines_engine::RouteChanges update_details;
  const auto &new_rpd = new_guidelines.rpd_;

  std::vector<std::string> updated_destination_classes;
  for (auto [i, name] : stdx::views::enumerate(rpd_->dest_names)) {
    bool found{false};
    for (auto [j, other_name] : stdx::views::enumerate(new_rpd->dest_names)) {
      if (name == other_name) {
        found = true;
        if (rpd_->dest_rules[i] != new_rpd->dest_rules[j]) {
          updated_destination_classes.push_back(name);
        }
        break;
      }
    }
    if (!found) {
      updated_destination_classes.push_back(name);
    }
  }

  // Check if there are destination changes
  for (const auto &old_route : rpd_->routes) {
    bool route_destinations_changed{false};
    for (const auto &dest_group : old_route.destination_groups) {
      for (const auto &dest : dest_group.destination_classes) {
        if (stdx::ranges::contains(updated_destination_classes, dest)) {
          // At least one of the destinations for the given route has changed
          update_details.affected_routes.push_back(old_route.name);
          route_destinations_changed = true;
        }
      }
    }

    if (!route_destinations_changed) {
      bool found{false};
      // Check if there are route (match or destination classes) changes
      for (const auto &new_route : new_rpd->routes) {
        if (old_route.name == new_route.name) {
          if (*old_route.match == *new_route.match &&
              old_route.destination_groups == new_route.destination_groups &&
              old_route.enabled == new_route.enabled &&
              old_route.connection_sharing_allowed ==
                  new_route.connection_sharing_allowed) {
            found = true;
            break;
          }
        }
      }
      if (!found) {
        update_details.affected_routes.push_back(old_route.name);
      }
    }
  }

  update_details.guideline_name = new_guidelines.name();
  return update_details;
}

bool Routing_guidelines_engine::routing_guidelines_updated() const {
  return rpd_->guidelines_updated;
}

Routing_guidelines_engine::RouteChanges
Routing_guidelines_engine::update_routing_guidelines(
    Routing_guidelines_engine &&new_rp, bool is_provided_by_user) {
  RouteChanges changes = compare(new_rp);
  std::swap(rpd_, new_rp.rpd_);
  rpd_->guidelines_updated = is_provided_by_user;

  // Do not report back name of the default guideline
  if (!is_provided_by_user) changes.guideline_name = "";

  return changes;
}

Routing_guidelines_engine::RouteChanges
Routing_guidelines_engine::restore_default() {
  auto new_guidelines_engine =
      Routing_guidelines_document_parser()(default_routing_guidelines_doc_);

  new_guidelines_engine.default_routing_guidelines_doc_ =
      default_routing_guidelines_doc_;

  return update_routing_guidelines(std::move(new_guidelines_engine),
                                   /*is_provided_by_user*/ false);
}

Routing_guidelines_engine::Route_classification
Routing_guidelines_engine::classify(const Session_info &session_info,
                                    const Router_info &router_info,
                                    const Sql_info *sql_info) const {
  Route_classification ret;
  rpn::Context context;
  context.set_session_info(session_info);
  context.set_router_info(router_info);
  if (sql_info != nullptr) context.set_sql_info(*sql_info);

  for (const auto &route : rpd_->routes) {
    if (!route.enabled) continue;
    try {
      auto res = route.match->eval(&context, &rpd_->cache);
      if (res.get_bool()) {
        ret.route_name = route.name;
        ret.destination_groups = route.destination_groups;
        ret.connection_sharing_allowed = route.connection_sharing_allowed;
        break;
      }
    } catch (const std::exception &e) {
      ret.errors.emplace_front("route." + route.name + ": " + e.what());
    }
  }
  return ret;
}

Routing_guidelines_engine::Destination_classification
Routing_guidelines_engine::classify(const Server_info &server_info,
                                    const Router_info &router_info) const {
  Destination_classification ret;
  rpn::Context context;
  context.set_server_info(server_info);
  context.set_router_info(router_info);

  for (const auto [i, dest_rule] : stdx::views::enumerate(rpd_->dest_rules)) {
    try {
      auto res = dest_rule.eval(&context, &rpd_->cache);
      if (res.get_bool()) ret.class_names.emplace_back(rpd_->dest_names[i]);
    } catch (const std::exception &e) {
      ret.errors.emplace_front("destinations." + rpd_->dest_names[i] + ": " +
                               e.what());
    }
  }
  return ret;
}

const std::string &Routing_guidelines_engine::name() const {
  return rpd_->name;
}

const std::vector<std::string> &Routing_guidelines_engine::destination_classes()
    const {
  return rpd_->dest_names;
}

const std::vector<Routing_guidelines_engine::Route>
    &Routing_guidelines_engine::get_routes() const {
  return rpd_->routes;
}

std::vector<Resolve_host> Routing_guidelines_engine::hostnames_to_resolve()
    const {
  return rpd_->hostnames_to_resolve;
}

void Routing_guidelines_engine::update_resolve_cache(ResolveCache cache) {
  rpd_->cache = std::move(cache);
}

void Routing_guidelines_engine::validate_one_destination(
    const std::string &destination) {
  rapidjson::Document doc;
  rapidjson::ParseResult ok = doc.Parse(destination.c_str());
  if (!ok) {
    throw std::runtime_error(format_json_error(destination, ok, 15));
  } else if (!doc.IsObject()) {
    throw std::runtime_error(
        "destination needs to be specified as a JSON document");
  } else {
    Routing_guidelines_document_parser().validate_rule(doc.GetObject());
  }
}

void Routing_guidelines_engine::validate_one_route(const std::string &route) {
  rapidjson::Document doc;
  rapidjson::ParseResult ok = doc.Parse(route.c_str());
  if (!ok) {
    throw std::runtime_error(format_json_error(route, ok, 15));
  } else if (!doc.IsObject()) {
    throw std::runtime_error("route needs to be specified as a JSON document");
  } else {
    Routing_guidelines_document_parser().validate_route(doc.GetObject());
  }
}

void Routing_guidelines_engine::validate_guideline_document(
    const std::string &document) {
  Routing_guidelines_document_parser()(document);
}

const rapidjson::Document &
Routing_guidelines_engine::get_routing_guidelines_document() const {
  return routing_guidelines_document_;
}

bool Routing_guidelines_engine::extended_session_info_in_use() const {
  return rpd_->extended_session_info_in_use;
}

bool Routing_guidelines_engine::session_rand_used() const {
  return rpd_->session_rand_used;
}

Routing_guidelines_engine::Route::Route(
    std::string name_, std::unique_ptr<rpn::Expression> match_,
    std::vector<DestinationGroup> destination_groups_,
    const std::optional<bool> connection_sharing_allowed_, const bool enabled_)
    : name(std::move(name_)),
      match(std::move(match_)),
      destination_groups(std::move(destination_groups_)),
      connection_sharing_allowed(connection_sharing_allowed_),
      enabled(enabled_) {}

std::string Routing_guidelines_engine::get_schema() {
  auto custom_to_string = [](const auto &vec) {
    std::ostringstream oss;
    oss << "[";
    if (!vec.empty()) {
      oss << "\"" << vec[0] << "\"";
      for (size_t i = 1; i < vec.size(); ++i) {
        oss << ",\"" << vec[i] << "\"";
      }
    }
    oss << "]";
    return oss.str();
  };

  return mysql_harness::utility::string_format(
      k_routing_guidelines_schema,
      custom_to_string(Rules_parser::get_keyword_names()).c_str(),
      custom_to_string(Rules_parser::get_function_names()).c_str(),
      custom_to_string(rpn::get_variables_names()).c_str());
}

}  // namespace routing_guidelines
