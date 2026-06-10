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

#include "rpn.h"

#include <cassert>
#include <cmath>  // fabs, fmod
#include <limits>
#include <mutex>
#include <regex>
#include <sstream>
#include <utility>

#include "utils.h"

namespace routing_guidelines {
namespace rpn {

class Regex_store {
 public:
  std::regex &get_regex(size_t idx) {
    assert(regexes.size() > idx);
    return regexes[idx];
  }

  int add_regex(const std::string &s) {
    std::lock_guard<std::mutex> guard(store_mutex);
    const auto it = regex_map.find(s);
    if (it != regex_map.end()) return it->second;

    const auto ret = regexes.size();
    regexes.emplace_back(s,
                         std::regex_constants::icase | std::regex::ECMAScript);
    regex_map.emplace(s, ret);
    return ret;
  }

 private:
  std::vector<std::regex> regexes;
  std::unordered_map<std::string, size_t> regex_map;
  std::mutex store_mutex;
};

static Regex_store g_regex_store;

Token Token::regexp(const std::string &rgx) {
  return Token(g_regex_store.add_regex(rgx), rpn::Token::Type::REGEXP);
}

bool Token::get_bool(const char *exception_msg) const {
  switch (type_) {
    case Type::NUM:
    case Type::BOOL:
      return std::fabs(std::get<double>(value_)) >
             std::numeric_limits<double>::epsilon();
    case Type::ROLE:
      return !str_caseeq(std::get<std::string>(value_), kUndefinedRole);
    case Type::NONE:
      return false;
    case Type::STR:
      return !std::get<std::string>(value_).empty();
    default:
      break;
  }
  throw std::runtime_error(exception_msg
                               ? exception_msg
                               : "Type error, expected boolean, but got: " +
                                     to_string(*this));
}

namespace {
bool check_nulls(const Token &lhs, const Token &rhs, bool bools = false) {
  if (lhs.is_null() || rhs.is_null()) return false;
  if (bools && (lhs.is_bool() || rhs.is_bool())) {
    try {
      return lhs.get_bool() == rhs.get_bool();
    } catch (...) {
    }
  }
  throw std::runtime_error("Incompatible operands for comparison: " +
                           to_string(lhs) + " vs " + to_string(rhs));
}
}  // namespace

bool operator==(const Token &lhs, const Token &rhs) {
  if (lhs.type_ != rhs.type_) return check_nulls(lhs, rhs, true);
  switch (lhs.type_) {
    case Token::Type::NUM:
    case Token::Type::BOOL:
      return std::get<double>(lhs.value_) == std::get<double>(rhs.value_);
    case Token::Type::NONE:
      return true;
    case Token::Type::ROLE:
    case Token::Type::STR:
      return str_caseeq(std::get<std::string>(lhs.value_),
                        std::get<std::string>(rhs.value_));
    default:
      break;
  }
  throw std::runtime_error("Token type not suitable for comparison: " +
                           to_string(lhs));
}

bool operator<(const Token &lhs, const Token &rhs) {
  if (lhs.type_ != rhs.type_)
    return check_nulls(lhs, rhs);
  else if (lhs.is_num())
    return std::get<double>(lhs.value_) < std::get<double>(rhs.value_);
  else if (lhs.is_string())
    return str_casecmp(std::get<std::string>(lhs.value_),
                       std::get<std::string>(rhs.value_)) < 0;
  else if (lhs.is_null())
    return false;
  else
    throw std::runtime_error("Only strings and numbers can be compared");
}

bool operator<=(const Token &lhs, const Token &rhs) {
  if (lhs.type_ != rhs.type_)
    return check_nulls(lhs, rhs);
  else if (lhs.is_num())
    return std::get<double>(lhs.value_) <= std::get<double>(rhs.value_);
  else if (lhs.is_string())
    return str_casecmp(std::get<std::string>(lhs.value_),
                       std::get<std::string>(rhs.value_)) <= 0;
  else if (lhs.is_null())
    return false;
  else
    throw std::runtime_error("Only strings and numbers can be compared");
}

std::string to_string(const Token::Type tt) {
  switch (tt) {
    case Token::Type::LT:
      return "<";
    case Token::Type::GT:
      return ">";
    case Token::Type::NE:
      return "<>";
    case Token::Type::LE:
      return "<=";
    case Token::Type::GE:
      return ">=";
    case Token::Type::EQ:
      return "=";
    case Token::Type::NUM:
      return "NUMBER";
    case Token::Type::BOOL:
      return "BOOLEAN";
    case Token::Type::STR:
      return "STRING";
    case Token::Type::NONE:
      return "NULL";
    case Token::Type::LIST:
      return "LIST";
    case Token::Type::ADD:
      return "+";
    case Token::Type::MIN:
      return "-";
    case Token::Type::DIV:
      return "/";
    case Token::Type::MUL:
      return "*";
    case Token::Type::MOD:
      return "%";
    case Token::Type::NEG:
      return "-";
    case Token::Type::TAG_REF:
      return "TAG_REF";
    case Token::Type::VAR_REF:
      return "VAR_REF";
    case Token::Type::IN_OP:
      return "IN";
    case Token::Type::NOT:
      return "NOT";
    case Token::Type::AND:
      return "AND";
    case Token::Type::OR:
      return "OR";
    case Token::Type::MID_OR:
      return "MID_OR";
    case Token::Type::MID_AND:
      return "MID_AND";
    case Token::Type::ROLE:
      return "ROLE";
    case Token::Type::FUNC:
      return "FUNCTION";
    case Token::Type::RESOLVE_V4:
      return "RESOLVE_V4";
    case Token::Type::RESOLVE_V6:
      return "RESOLVE_V6";
    case Token::Type::CONCAT:
      return "CONCAT";
    case Token::Type::REGEXP:
      return "REGEXP";
    case Token::Type::NETWORK:
      return "NETWORK";
  }
  return "UNKNOWN_TOKEN";
}

std::string to_string(const Token &token, bool print_value) {
  const auto tok_str = [&print_value, &token](const auto &val) -> std::string {
    if (!print_value) return "'" + to_string(token.type()) + "'";
    std::ostringstream ss;
    ss << "'" << to_string(token.type()) << "'";
    ss << std::boolalpha << "(" << val << ")";
    return ss.str();
  };

  switch (token.type()) {
    case Token::Type::NUM:
      return tok_str(token.number());
    case Token::Type::BOOL:
      return tok_str(token.get_bool());
    case Token::Type::STR:
      return tok_str(token.string());
    case Token::Type::LIST:
      return tok_str(token.number());
    case Token::Type::TAG_REF:
      return tok_str(token.string());
    case Token::Type::VAR_REF:
      return tok_str(token.number());
    case Token::Type::MID_OR:
      return tok_str(token.number());
    case Token::Type::MID_AND:
      return tok_str(token.number());
    case Token::Type::ROLE:
      return tok_str(token.string());
    case Token::Type::RESOLVE_V4:
      return tok_str(token.string());
    case Token::Type::RESOLVE_V6:
      return tok_str(token.string());
    case Token::Type::CONCAT:
      return tok_str(token.number());
    case Token::Type::REGEXP:
      return tok_str(token.number());
    default:
      break;
  }
  return "'" + to_string(token.type()) + "'";
}

void Function_definition::reduce(std::vector<rpn::Token> *stack) const {
  assert(stack->size() >= args.size());
  bool nulls = false;
  const auto args_offset = stack->size() - args.size();
  auto &st = *stack;
  for (size_t i = 0; i < args.size(); i++) {
    if (st[args_offset + i].is_null())
      nulls = true;
    else if (st[args_offset + i].type() != args[i])
      throw std::runtime_error("Function " + std::string(name) +
                               " argument type mismatch");
  }

  if (nulls) {
    st.resize(st.size() - args.size());
    st.emplace_back();
  } else {
    reducer(stack);
    assert(st.back().type() == ret_val);
  }
}

enum class Guidelines_vars {
  local_cluster,
  router_hostname,
  router_bind_address,
  port_ro,
  port_rw,
  port_rw_split,
  route_name,
  name,
  server_label,
  server_address,
  server_port,
  uuid,
  server_version,
  cluster_name,
  clusterset_name,
  is_cluster_invalidated,
  member_role,
  cluster_role,
  target_ip,
  target_port,
  source_ip,
  session_rand,
  user,
  schema
};

const std::map<Guidelines_vars, std::string_view> &get_vars_names() {
  static const std::map<Guidelines_vars, std::string_view> vars_names{
      {Guidelines_vars::local_cluster, "router.localCluster"},
      {Guidelines_vars::router_hostname, "router.hostname"},
      {Guidelines_vars::router_bind_address, "router.bindAddress"},
      {Guidelines_vars::port_ro, "router.port.ro"},
      {Guidelines_vars::port_rw, "router.port.rw"},
      {Guidelines_vars::port_rw_split, "router.port.rw_split"},
      {Guidelines_vars::route_name, "router.routeName"},
      {Guidelines_vars::name, "router.name"},

      {Guidelines_vars::server_label, "server.label"},
      {Guidelines_vars::server_address, "server.address"},
      {Guidelines_vars::server_port, "server.port"},
      {Guidelines_vars::uuid, "server.uuid"},
      {Guidelines_vars::server_version, "server.version"},
      {Guidelines_vars::cluster_name, "server.clusterName"},
      {Guidelines_vars::clusterset_name, "server.clusterSetName"},
      {Guidelines_vars::is_cluster_invalidated, "server.isClusterInvalidated"},
      {Guidelines_vars::member_role, "server.memberRole"},
      {Guidelines_vars::cluster_role, "server.clusterRole"},

      {Guidelines_vars::target_ip, "session.targetIP"},
      {Guidelines_vars::target_port, "session.targetPort"},
      {Guidelines_vars::source_ip, "session.sourceIP"},
      {Guidelines_vars::session_rand, "session.randomValue"},
      {Guidelines_vars::user, "session.user"},
      {Guidelines_vars::schema, "session.schema"},
  };
  return vars_names;
}

Context::Context() {
  auto register_var = [this](auto &scope, auto var, std::string_view name) {
    context_vars_.emplace_back([this, &scope, var, name]() {
      if (scope == nullptr) return this->handle_miss(name);
      return Token(scope->*var);
    });
    context_.emplace(name, context_vars_.size() - 1);
  };

  auto register_role = [this](auto &scope, auto var, std::string_view name) {
    context_vars_.emplace_back([this, &scope, var, name]() {
      if (scope == nullptr) return this->handle_miss(name);
      return Token(
          (scope->*var).empty() ? kUndefinedRole : (scope->*var).c_str(),
          Token::Type::ROLE);
    });
    context_.emplace(name, context_vars_.size() - 1);
  };
  const auto vars_names = get_vars_names();

  register_var(router_, &Router_info::local_cluster,
               vars_names.at(Guidelines_vars::local_cluster));
  register_var(router_, &Router_info::bind_address,
               vars_names.at(Guidelines_vars::router_bind_address));
  register_var(router_, &Router_info::hostname,
               vars_names.at(Guidelines_vars::router_hostname));
  register_var(router_, &Router_info::port_ro,
               vars_names.at(Guidelines_vars::port_ro));
  register_var(router_, &Router_info::port_rw,
               vars_names.at(Guidelines_vars::port_rw));
  register_var(router_, &Router_info::port_rw_split,
               vars_names.at(Guidelines_vars::port_rw_split));
  register_var(router_, &Router_info::route_name,
               vars_names.at(Guidelines_vars::route_name));
  register_var(router_, &Router_info::name,
               vars_names.at(Guidelines_vars::name));

  register_var(server_, &Server_info::label,
               vars_names.at(Guidelines_vars::server_label));
  register_var(server_, &Server_info::address,
               vars_names.at(Guidelines_vars::server_address));
  register_var(server_, &Server_info::port,
               vars_names.at(Guidelines_vars::server_port));
  register_var(server_, &Server_info::uuid,
               vars_names.at(Guidelines_vars::uuid));
  register_var(server_, &Server_info::version,
               vars_names.at(Guidelines_vars::server_version));
  register_var(server_, &Server_info::cluster_name,
               vars_names.at(Guidelines_vars::cluster_name));
  register_var(server_, &Server_info::cluster_set_name,
               vars_names.at(Guidelines_vars::clusterset_name));
  register_var(server_, &Server_info::cluster_is_invalidated,
               vars_names.at(Guidelines_vars::is_cluster_invalidated));

  register_role(server_, &Server_info::member_role,
                vars_names.at(Guidelines_vars::member_role));
  register_role(server_, &Server_info::cluster_role,
                vars_names.at(Guidelines_vars::cluster_role));

  register_var(session_, &Session_info::target_ip,
               vars_names.at(Guidelines_vars::target_ip));
  register_var(session_, &Session_info::target_port,
               vars_names.at(Guidelines_vars::target_port));
  register_var(session_, &Session_info::source_ip,
               vars_names.at(Guidelines_vars::source_ip));
  register_var(session_, &Session_info::random_value,
               vars_names.at(Guidelines_vars::session_rand));
  register_var(session_, &Session_info::user,
               vars_names.at(Guidelines_vars::user));
  register_var(session_, &Session_info::schema,
               vars_names.at(Guidelines_vars::schema));
}

Token Context::get_tag(std::string_view name) const {
  // We check third character to discriminate the variable
  if (name.size() < 3) return handle_miss(name);

  auto get_tag_val = [this](const auto &ht, std::string_view tag) {
    auto i = ht.find(tag);
    if (i != ht.end()) return Token(i->second);
    return parse_mode_ ? Token("") : Token();
  };

  // name[2] is a first character distinguishing scopes with one comparison
  switch (name[2]) {
    case 'u': {
      const std::string router_pref{"router.tags."};
      if (router_ && name.starts_with(router_pref)) {
        return get_tag_val(router_->tags, name.substr(router_pref.size()));
      }
    } break;
    case 'r': {
      const std::string server_pref{"server.tags."};
      if (server_ && name.starts_with(server_pref)) {
        return get_tag_val(server_->tags, name.substr(server_pref.size()));
      }
    } break;
    case 's': {
      const std::string session_pref{"session.connectAttrs."};
      if (session_ && name.starts_with(session_pref)) {
        return get_tag_val(session_->connect_attrs,
                           name.substr(session_pref.size()));
      }
    } break;
    case 'l':
      if (sql_) {
        const std::string sqltags_pref{"sql.queryTags."};
        const std::string sqlhints_pref{"sql.queryHints."};
        if (name[9] == 'T' && name.starts_with(sqltags_pref)) {
          return get_tag_val(sql_->query_tags,
                             name.substr(sqltags_pref.size()));
        }
        if (name.starts_with(sqlhints_pref)) {
          return get_tag_val(sql_->query_hints,
                             name.substr(sqlhints_pref.size()));
        }
      }
      break;
  }
  return handle_miss(name);
}

Token Context::get(const std::string &name) const {
  auto it = context_.find(name);
  if (it != context_.end()) return context_vars_[it->second]();
  return get_tag(name);
}

Token::Type Context::get_type(std::string_view name, int *offset) const {
  auto it = context_.find(name);
  if (it != context_.end()) {
    *offset = it->second;
    return context_vars_[it->second]().type();
  }
  *offset = -1;
  return get_tag(name).type();
}

std::optional<std::string> Context::get_var_name(const Token &tok) const {
  assert(tok.type() == rpn::Token::Type::VAR_REF);

  auto pos = std::find_if(
      std::begin(context_), std::end(context_),
      [val = tok.number()](const auto &entry) { return val == entry.second; });

  if (pos != std::end(context_)) return pos->first;

  return std::nullopt;
}

bool Context::parse_tags_toggled() {
  if (parsing_tags_) {
    parsing_tags_ = false;
    return true;
  }
  return false;
}

Token Context::handle_miss(std::string_view name) const {
  if (parse_mode_) {
    throw std::runtime_error("undefined variable: " + std::string(name));
  }
  return Token();
}

#define MATH_OP(op, name)                                    \
  {                                                          \
    assert(stack.size() >= 2);                               \
    auto &o1 = stack.end()[-2];                              \
    if (!o1.is_null()) {                                     \
      if (!o1.is_num())                                      \
        throw std::runtime_error("left operand of " name     \
                                 " needs to be a number");   \
      auto &o2 = stack.back();                               \
      if (o2.is_null()) {                                    \
        o1 = o2;                                             \
      } else {                                               \
        if (!o2.is_num())                                    \
          throw std::runtime_error("right operand of " name  \
                                   " needs to be a number"); \
        stack.end()[-2].number() op stack.back().number();   \
      }                                                      \
    }                                                        \
    stack.pop_back();                                        \
    break;                                                   \
  }

namespace {
void reduce_network(std::vector<Token> *stack, int netmask) {
  const std::string &ip = stack->back().get_string();
  auto &last = stack->back();
  last = Token(network(ip, netmask));
}

void reduce_concat(std::vector<Token> *stack, int count) {
  bool all_strings{true}, nulls{false};
  for (size_t i = stack->size() - count; i < stack->size(); i++) {
    if (!(*stack)[i].is_string()) {
      all_strings = false;
      if ((*stack)[i].is_null()) {
        nulls = true;
        break;
      }
    }
  }

  auto &ret = (*stack)[stack->size() - count];
  if (nulls) {
    ret = Token();
  } else if (all_strings) {
    auto &str = ret.string();
    for (size_t i = stack->size() - count + 1; i < stack->size(); i++) {
      str += (*stack)[i].string();
    }
  } else {
    std::ostringstream ss;
    for (size_t i = stack->size() - count; i < stack->size(); i++) {
      const auto &tok = (*stack)[i];
      if (tok.is_string() || tok.is_role()) {
        ss << tok.string();
      } else if (tok.is_num()) {
        ss << tok.number();
      } else if (tok.is_bool()) {
        ss << tok.get_bool();
      } else {
        assert(false);
      }
    }
    ret = Token(ss.str());
  }
  stack->resize(stack->size() - count + 1);
}

}  // namespace

bool Expression::verify(Context *variables) const {
  return eval(variables, nullptr, true).is_bool();
}

Token Expression::eval(Context *variables,
                       const Routing_guidelines_engine::ResolveCache *cache,
                       const bool dry_run) const {
  std::vector<Token> stack;
  const auto comp_op =
      [&stack](
          const std::function<bool(const Token &lhs, const Token &rhs)> &op) {
        assert(stack.size() >= 2);
        bool res = op(stack.end()[-2], stack.back());
        stack.pop_back();
        stack.back() = Token(res);
      };

  for (size_t i = 0; i < rpn_.size(); i++) {
    const auto &tok = rpn_[i];
    try {
      switch (tok.type()) {
        case Token::Type::NUM:
        case Token::Type::STR:
        case Token::Type::BOOL:
        case Token::Type::LIST:
        case Token::Type::NONE:
        case Token::Type::ROLE:
          stack.emplace_back(tok);
          break;
        case Token::Type::NEG:
          if (!stack.back().is_null()) {
            if (!stack.back().is_num())
              throw std::runtime_error("only numbers can be negated");
            stack.back().number() = -stack.back().number();
          }
          break;
        case Token::Type::ADD:
          MATH_OP(+=, "addition");
        case Token::Type::MIN:
          MATH_OP(-=, "subtraction");
        case Token::Type::MUL:
          MATH_OP(*=, "multiplication");
        case Token::Type::DIV:
          MATH_OP(/=, "division");
        case Token::Type::MOD: {
          auto &o1 = stack.end()[-2];
          if (!o1.is_null()) {
            if (!o1.is_num())
              throw std::runtime_error(
                  "left operand of modulo needs to be a number");
            auto &o2 = stack.back();
            if (o2.is_null()) {
              o1 = o2;
            } else {
              if (!o2.is_num())
                throw std::runtime_error(
                    "right operand of modulo needs to be a number");
              stack.end()[-2].number() =
                  std::fmod(stack.end()[-2].number(), stack.back().number());
            }
          }
          stack.pop_back();
          break;
        }
        case Token::Type::TAG_REF: {
          stack.emplace_back(variables->get_tag(tok.get_string()));
          break;
        }
        case Token::Type::VAR_REF: {
          stack.emplace_back(variables->get(tok.number()));
          break;
        }
        case Token::Type::LT:
          comp_op(operator<);  // NOLINT
          break;
        case Token::Type::GT:
          comp_op(operator>);  // NOLINT
          break;
        case Token::Type::LE:
          comp_op(operator<=);  // NOLINT
          break;
        case Token::Type::GE:
          comp_op(operator>=);  // NOLINT
          break;
        case Token::Type::EQ:
          comp_op(static_cast<bool (*)(const Token &, const Token &)>(
              &operator==));  // NOLINT
          break;
        case Token::Type::NE:
          comp_op(static_cast<bool (*)(const Token &, const Token &)>(
              operator!=));  // NOLINT
          break;
        case Token::Type::IN_OP: {
          size_t s = 1;
          if (stack.back().type() == Token::Type::LIST) {
            s = static_cast<size_t>(stack.back().number());
            stack.pop_back();
          }
          assert(stack.size() > s);
          const auto &needle = stack[stack.size() - s - 1];
          bool found = false;
          for (size_t j = 1; !found && j <= s; j++)
            if (needle == stack[stack.size() - j]) found = true;
          stack.resize(stack.size() - s);
          stack.back() = Token(found);
          break;
        }
        case Token::Type::NOT: {
          assert(stack.size() >= 1);
          stack.back() = Token(
              !stack.back().get_bool("NOT operator expects boolean argument"));
          break;
        }
        case Token::Type::AND: {
          assert(stack.size() >= 2);
          bool res = stack.end()[-2].get_bool(
                         "left operand of AND needs to be a boolean") &&
                     stack.back().get_bool(
                         "right operand of AND needs to be a boolean");
          stack.pop_back();
          stack.back() = Token(res);
          break;
        }
        case Token::Type::MID_AND: {
          assert(stack.size() >= 1);
          if (stack.back().get_bool(
                  "left operand of AND needs to be a boolean") == false) {
            i += tok.number();
          }
          break;
        }
        case Token::Type::OR: {
          assert(stack.size() >= 2);
          bool res = stack.end()[-2].get_bool(
                         "left operand of OR needs to be a boolean") ||
                     stack.back().get_bool(
                         "right operand of OR needs to be a boolean");
          stack.pop_back();
          stack.back() = Token(res);
          break;
        }
        case Token::Type::MID_OR: {
          assert(stack.size() >= 1);
          if (stack.back().get_bool(
                  "left operand of OR needs to be a boolean") == true) {
            i += tok.number();
          }
          break;
        }
        case Token::Type::FUNC:
          tok.function().reduce(&stack);
          break;
        case Token::Type::RESOLVE_V6:
          if (dry_run) {
            stack.emplace_back(tok.string());
            break;
          }
          if (cache != nullptr) {
            const auto it = cache->find(tok.string());
            if (it != cache->end() && it->second.is_v6()) {
              stack.emplace_back(it->second.to_string());
              continue;
            }
          }
          throw std::runtime_error("No cache entry to resolve host: " +
                                   tok.string());
          break;
        case Token::Type::RESOLVE_V4:
          if (dry_run) {
            stack.emplace_back(tok.string());
            break;
          }
          if (cache != nullptr) {
            const auto it = cache->find(tok.string());
            if (it != cache->end() && it->second.is_v4()) {
              stack.emplace_back(it->second.to_string());
              continue;
            }
          }
          throw std::runtime_error("No cache entry to resolve host: " +
                                   tok.string());
          break;
        case Token::Type::CONCAT:
          reduce_concat(&stack, tok.number());
          break;
        case Token::Type::REGEXP:
          assert(stack.size() > 0);
          if (!stack.back().is_null()) {
            stack.back() = rpn::Token(
                std::regex_match(stack.back().get_string(),
                                 g_regex_store.get_regex(tok.number())));
          }
          break;
        case Token::Type::NETWORK:
          if (dry_run) {
            const int netmask = tok.number();
            if (netmask < 1 || netmask > 32) {
              throw std::runtime_error(
                  std::string{"NETWORK function invalid netmask value: "} +
                  std::to_string(netmask));
            }

            stack.back() = Token(std::to_string(tok.number()));
          } else {
            reduce_network(&stack, tok.number());
          }
          break;
      }
    } catch (const std::exception &e) {
      if (!tok.has_location()) throw;
      const auto &loc = tok.location();
      throw std::runtime_error(error_msg(e.what(), code_, loc.start, loc.end));
    }
  }
  assert(stack.size() <= 1);
  if (stack.empty()) return Token();
  return stack.back();
}

#undef MATH_OP

void Expression::clear() {
  rpn_.clear();
  code_.clear();
}

bool operator==(const Expression &l, const Expression &r) {
  const auto &lhs = l.rpn_;
  const auto &rhs = r.rpn_;

  if (lhs.size() != rhs.size()) return false;
  for (size_t i = 0; i < lhs.size(); i++) {
    if (lhs[i].type() != rhs[i].type()) return false;

    auto find_mismatch = [&rhs_val = rhs[i]](auto &&arg) -> bool {
      using T = std::decay_t<decltype(arg)>;
      if constexpr (std::is_same_v<T, double>) {
        return arg != rhs_val.get_num();
      } else if constexpr (std::is_same_v<T, std::string>) {
        return !str_caseeq(arg, rhs_val.get_string());
      } else if constexpr (std::is_same_v<T, Token::Function>) {
        return arg.definition != &rhs_val.function();
      }
      return false;
    };

    const auto mismatched = lhs[i].visit(find_mismatch);
    if (mismatched) return false;
  }
  return true;
}

bool operator!=(const Expression &lhs, const Expression &rhs) {
  return !(lhs == rhs);
}

std::string error_msg(const char *msg, const std::string &exp, int beg,
                      int end) {
  std::string ret{msg};
  if (ret.back() == '.') ret.back() = ',';

  if (end - beg < 2)
    ret += " (character " + std::to_string(beg + 1) + ")";
  else
    ret += " in '" + exp.substr(beg, end - beg) + "'";

  return ret;
}

std::vector<std::string_view> get_variables_names() {
  std::vector<std::string_view> result;
  for (const auto &var : get_vars_names()) result.push_back(var.second);
  return result;
}

}  // namespace rpn
}  // namespace routing_guidelines
