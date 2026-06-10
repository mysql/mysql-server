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

#ifndef ROUTER_SRC_ROUTING_GUIDELINES_SRC_RULES_PARSER_H_
#define ROUTER_SRC_ROUTING_GUIDELINES_SRC_RULES_PARSER_H_

#include <array>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

#include "rpn.h"

namespace routing_guidelines {
class Rules_parser;

const std::array<std::string_view, 4> k_member_roles{
    kUndefinedRole, "PRIMARY", "SECONDARY", "READ_REPLICA"};

const std::array<std::string_view, 3> k_cluster_roles{kUndefinedRole, "PRIMARY",
                                                      "REPLICA"};

const std::array<std::string_view, 2> k_routing_strategies{"round-robin",
                                                           "first-available"};

bool is_member_role(const std::string &s);

bool is_cluster_role(const std::string &s);

struct Exp_info {
  int toks;
  rpn::Token::Type type;
};

using List_info = std::vector<Exp_info>;

}  // namespace routing_guidelines

union YYSTYPE;
struct YYLTYPE;

int yyparse(routing_guidelines::Rules_parser *rp);
int yylex(union YYSTYPE *lvalp, YYLTYPE *llocp,
          routing_guidelines::Rules_parser *rp);
void yyerror(YYLTYPE *llocp, routing_guidelines::Rules_parser *rp,
             const char *msg);

namespace routing_guidelines {

/// Conducting the whole scanning and parsing of routing guidelines rules
class Rules_parser final {
 public:
  explicit Rules_parser(
      std::function<void(const std::string &)> tracer = nullptr)
      : tracer_(tracer) {}

  rpn::Expression parse(std::string buf, rpn::Context *context);

  bool extended_session_info_used() const {
    return extended_session_info_in_use_;
  }

  bool session_rand_used() const { return session_rand_value_used_; }

  static std::vector<std::string> get_keyword_names();
  static std::vector<std::string> get_function_names();

 private:
  Rules_parser(const Rules_parser &) = delete;
  Rules_parser(Rules_parser &&) = delete;
  Rules_parser &operator=(const Rules_parser &) = delete;
  Rules_parser &operator=(Rules_parser &&) = delete;

  // RPN creation
  void emit(rpn::Token::Type type, const YYLTYPE &loc);
  void emit_null();
  void emit_num(double val, rpn::Token::Type type = rpn::Token::Type::NUM);
  void emit_string(std::string_view str,
                   rpn::Token::Type type = rpn::Token::Type::STR);
  void emit_log_operation(rpn::Token::Type type, double arg_split,
                          const YYLTYPE &loc);
  bool emit_in_op(const Exp_info &e, List_info *list, YYLTYPE *llocp,
                  Exp_info *ret);
  bool emit_like_op(const Exp_info &str, const Exp_info &pattern,
                    YYLTYPE *llocp, Exp_info *ret);
  bool emit_function(const rpn::Function_definition *function,
                     List_info *arguments, YYLTYPE *llocp, Exp_info *ret);
  bool emit_resolve(YYLTYPE *llocp, Exp_info *ret,
                    rpn::Token::Type resolve_ver);
  bool emit_concat(const List_info *arguments, YYLTYPE *llocp, Exp_info *ret);
  bool emit_regexp(const List_info *arguments, YYLTYPE *llocp, Exp_info *ret);
  bool emit_reference(std::string_view name, YYLTYPE *llocp, Exp_info *ret);
  bool emit_network(Exp_info *ret);

  bool check_role_types(const Exp_info &left, const Exp_info &right,
                        YYLTYPE *llocp);

  bool type_error(YYLTYPE *llocp, const std::string &msg,
                  rpn::Token::Type expected, rpn::Token::Type got);

  void trace(const std::string &s) {
    if (tracer_) tracer_(s);
  }

  void is_extended_session_info(std::string_view name);

  void uses_session_rand(std::string_view name);

  // Parser state
  std::vector<rpn::Token> rpn_;
  std::string error_;
  rpn::Context *context_{nullptr};
  bool extended_session_info_in_use_{false};
  bool session_rand_value_used_{false};

  // Scanner state
  std::string buf_;
  std::string::size_type pos_{0};

  // Function for trace output
  std::function<void(const std::string &)> tracer_;

  // friend yy::Parser::symbol_type yylex(Rules_parser *rp);
  friend int ::yyparse(Rules_parser *rp);
  friend int ::yylex(union YYSTYPE *lvalp, YYLTYPE *llocp,
                     routing_guidelines::Rules_parser *rp);
  friend void ::yyerror(YYLTYPE *llocp, routing_guidelines::Rules_parser *rp,
                        const char *msg);
};
}  // namespace routing_guidelines
#endif  // ROUTER_SRC_ROUTING_GUIDELINES_SRC_RULES_PARSER_H_
