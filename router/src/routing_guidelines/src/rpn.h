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

#ifndef ROUTER_SRC_ROUTING_GUIDELINES_SRC_RPN_H_
#define ROUTER_SRC_ROUTING_GUIDELINES_SRC_RPN_H_

#include <cmath>
#include <concepts>
#include <exception>
#include <functional>
#include <memory>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

#include "mysqlrouter/routing_guidelines_version.h"
#include "routing_guidelines/routing_guidelines.h"

namespace routing_guidelines {
class Rules_parser;
class Routing_guidelines_parser_test;
class Routing_guidelines_document_parser;

namespace rpn {

struct Function_definition;

class Token final {
 public:
  enum class Type {
    NUM,
    STR,
    BOOL,
    ROLE,
    LIST,
    NONE,
    ADD,
    MIN,
    DIV,
    MUL,
    MOD,
    NEG,
    LT,
    GT,
    NE,
    LE,
    GE,
    EQ,
    IN_OP,
    NOT,
    AND,
    MID_AND,
    OR,
    MID_OR,
    TAG_REF,
    VAR_REF,
    FUNC,
    RESOLVE_V4,
    RESOLVE_V6,
    CONCAT,
    REGEXP,
    NETWORK
  };

  struct Location {
    int start;
    int end;
  };

  struct Function {
    const Function_definition *definition;
    Location location;
  };

  Token() : type_(Type::NONE), value_(0.0) {}

  Token(Type type, int start, int end)
      : type_(type), value_(Location{start, end}) {}

  template <typename T>
    requires std::is_arithmetic_v<T>
  explicit Token(T v, Type type = Type::NUM)
      : type_(type), value_(static_cast<double>(v)) {}

  template <typename T>
    requires std::is_constructible_v<std::string, T>
  explicit Token(T &&s, Type type = Type::STR)
      : type_(type), value_(std::string(std::forward<T>(s))) {}

  explicit Token(bool val) : type_(Type::BOOL), value_(val ? 1.0 : 0.0) {}

  Token(const Function_definition &function, int start, int end)
      : type_(Type::FUNC), value_(Function{&function, {start, end}}) {}

  Token(const Token &i) = default;
  Token(Token &&i) noexcept = default;

  static Token regexp(const std::string &rgx);

  Token &operator=(const Token &i) = default;
  Token &operator=(Token &&i) noexcept = default;

  bool is_num() const { return type_ == Type::NUM; }

  bool is_role() const { return type_ == Type::ROLE; }

  double get_num() const { return std::get<double>(value_); }

  bool is_bool() const { return type_ == Type::BOOL; }

  bool get_bool(const char *exception_msg = nullptr) const;

  bool is_string() const { return type_ == Type::STR; }

  const std::string &get_string() const {
    if (std::holds_alternative<std::string>(value_)) {
      return std::get<std::string>(value_);
    }
    throw std::runtime_error("Type error, expected string");
  }

  bool is_null() const { return type_ == Type::NONE; }

  double &number() { return std::get<double>(value_); }

  const double &number() const { return std::get<double>(value_); }

  const std::string &string() const { return std::get<std::string>(value_); }

  std::string &string() { return std::get<std::string>(value_); }

  bool has_location() const {
    return std::holds_alternative<Location>(value_) ||
           std::holds_alternative<Function>(value_);
  }

  const Location &location() const {
    return std::holds_alternative<Function>(value_)
               ? std::get<Function>(value_).location
               : std::get<Location>(value_);
  }

  const Function_definition &function() const {
    return *std::get<Function>(value_).definition;
  }

  Type type() const { return type_; }

  template <class Visitor>
  constexpr auto visit(Visitor &&vis) const {
    return std::visit(vis, value_);
  }

 private:
  Type type_;

  std::variant<double, std::string, Location, Function> value_;

  friend bool operator==(const Token &lhs, const Token &rhs);
  friend bool operator<(const Token &lhs, const Token &rhs);
  friend bool operator<=(const Token &lhs, const Token &rhs);
};

inline bool operator!=(const Token &lhs, const Token &rhs) {
  return !(lhs == rhs);
}
inline bool operator>(const Token &lhs, const Token &rhs) { return rhs < lhs; }
inline bool operator>=(const Token &lhs, const Token &rhs) {
  return rhs <= lhs;
}

std::string to_string(const Token::Type tt);
std::string to_string(const Token &token, bool print_value = false);

struct Function_definition {
  const char *name;
  std::vector<rpn::Token::Type> args;
  rpn::Token::Type ret_val;
  void (*reducer)(std::vector<Token> *);

  void reduce(std::vector<rpn::Token> *stack) const;
};

class Context final {
 public:
  Context();
  Context(const Context &) = delete;
  Context &operator=(const Context &) = delete;

  Token get_tag(std::string_view name) const;
  Token get(const std::string &name) const;
  Token get(int offset) const { return context_vars_[offset](); }
  Token::Type get_type(std::string_view name, int *offset) const;
  std::optional<std::string> get_var_name(const Token &tok) const;

  template <class T>
  void set(const std::string &name, T &&value) {
    context_vars_.emplace_back(
        [t = Token(std::forward<T>(value))]() { return t; });
    context_[name] = context_vars_.size() - 1;
  }

  void set_server_info(const routing_guidelines::Server_info &server_info) {
    server_ = &server_info;
  }

  void clear_server_info() { server_ = nullptr; }

  void set_session_info(const routing_guidelines::Session_info &session_info) {
    session_ = &session_info;
  }

  void clear_session_info() { session_ = nullptr; }

  void set_sql_info(const routing_guidelines::Sql_info &sql_info) {
    sql_ = &sql_info;
  }

  void clear_sql_info() { sql_ = nullptr; }

  void set_router_info(const routing_guidelines::Router_info &router_info) {
    router_ = &router_info;
  }

  void clear_router_info() { router_ = nullptr; }

  bool parse_tags_toggled();

  mysqlrouter::RoutingGuidelinesVersion get_version() const { return version_; }

  void set_version(mysqlrouter::RoutingGuidelinesVersion version) {
    version_ = std::move(version);
  }

 private:
  std::unique_ptr<bool, std::function<void(bool *)>> start_parse_mode() {
    parse_mode_ = true;
    return std::unique_ptr<bool, std::function<void(bool *)>>(
        &parse_mode_, [](bool *b) { *b = false; });
  }

  Token handle_miss(std::string_view name) const;

  const routing_guidelines::Router_info *router_{nullptr};
  const routing_guidelines::Server_info *server_ = nullptr;
  const routing_guidelines::Session_info *session_ = nullptr;
  const routing_guidelines::Sql_info *sql_ = nullptr;

  std::map<std::string, int, std::less<>> context_;
  std::vector<std::function<Token()>> context_vars_;
  bool parse_mode_{false};
  bool extended_session_info_{false};
  bool parsing_tags_{false};
  mysqlrouter::RoutingGuidelinesVersion version_{
      mysqlrouter::kBaseRoutingGuidelines};

  friend class routing_guidelines::Rules_parser;
};

class Expression {
 public:
  Expression() = default;
  Expression(const std::vector<Token> &rpn, std::string code)
      : rpn_(rpn), code_(std::move(code)) {}
  Expression(std::vector<Token> &&rpn, std::string code)
      : rpn_(std::move(rpn)), code_(std::move(code)) {}

  Token eval(Context *variables,
             const Routing_guidelines_engine::ResolveCache *cache = nullptr,
             const bool dry_run = false) const;

  bool verify(Context *variables) const;

  std::string variable() const { return rpn_.back().string(); }

  bool empty() { return rpn_.empty(); }
  void clear();

  friend bool operator==(const Expression &lhs, const Expression &rhs);
  friend bool operator!=(const Expression &lhs, const Expression &rhs);

 private:
  std::vector<Token> rpn_;
  std::string code_;

  friend Routing_guidelines_parser_test;
  friend Routing_guidelines_document_parser;
};

std::string error_msg(const char *msg, const std::string &exp, int beg,
                      int end);

std::vector<std::string_view> get_variables_names();

}  // namespace rpn
}  // namespace routing_guidelines

#endif  // ROUTER_SRC_ROUTING_GUIDELINES_SRC_RPN_H_
