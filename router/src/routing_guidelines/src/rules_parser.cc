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

#include "rules_parser.h"

#include <algorithm>
#include <cctype>
#include <exception>
#include <memory>
#include <regex>
#include <unordered_map>
#include <utility>

#include "parser.h"
#include "utils.h"  // case_contains is_ipv4 is_ipv6

#define NUMT rpn::Token::Type::NUM
#define STRT rpn::Token::Type::STR
#define BOOLT rpn::Token::Type::BOOL

namespace routing_guidelines {

bool is_member_role(const std::string &s) {
  return case_contains(k_member_roles, s);
}

bool is_cluster_role(const std::string &s) {
  return case_contains(k_cluster_roles, s);
}

namespace {

void reduce_regexp_like(std::vector<rpn::Token> *stack) {
  auto &op1 = stack->end()[-2];
  op1 = rpn::Token(std::regex_match(
      op1.get_string(),
      std::regex(stack->back().get_string(),
                 std::regex_constants::icase | std::regex::ECMAScript)));
  stack->pop_back();
}

void reduce_is_ipv4(std::vector<rpn::Token> *stack) {
  auto &back = stack->back();
  back = rpn::Token(is_ipv4(back.string()));
}

void reduce_is_ipv6(std::vector<rpn::Token> *stack) {
  auto &back = stack->back();
  back = rpn::Token(is_ipv6(back.string()));
}

void reduce_sqrt(std::vector<rpn::Token> *stack) {
  auto &op = stack->back().number();
  op = sqrt(op);
}

void reduce_number(std::vector<rpn::Token> *stack) {
  auto &op = stack->back();
  char *end = const_cast<char *>(op.string().c_str());
  double num = strtod(end, &end);
  if (*end != '\0')
    throw std::runtime_error("NUMBER function, unable to convert '" +
                             op.string() + "' to number");
  op = rpn::Token(num);
}

void reduce_substring_index(std::vector<rpn::Token> *stack) {
  int index = stack->back().number();
  const std::string delim = stack->end()[-2].string();
  auto &str = stack->end()[-3].string();
  if (index == 0) {
    str.clear();
  } else if (index < 0) {
    size_t pos = str.rfind(delim);
    for (int i = 1; pos != std::string::npos && i < -index; i++) {
      pos = str.rfind(delim, pos - 1);
    }
    if (pos != std::string::npos) str = str.substr(pos + delim.size());
  } else {
    size_t pos = str.find(delim);
    for (int i = 1; pos != std::string::npos && i < index; i++) {
      pos = str.find(delim, pos + 1);
    }
    if (pos != std::string::npos) str = str.substr(0, pos);
  }
  stack->resize(stack->size() - 2);
}

void reduce_str_startswith(std::vector<rpn::Token> *stack) {
  auto &op1 = stack->end()[-2];
  op1 = rpn::Token(str_ibeginswith(op1.string(), stack->back().string()));
  stack->pop_back();
}

void reduce_str_endswith(std::vector<rpn::Token> *stack) {
  bool res = false;
  auto &op1 = stack->end()[-2];
  const auto &str = op1.string();
  const auto &suffix = stack->back().string();

  if (str.size() >= suffix.size())
    res = str_caseeq(str.c_str() + str.size() - suffix.size(), suffix.c_str());

  op1 = rpn::Token(res);
  stack->pop_back();
}

void reduce_str_contains(std::vector<rpn::Token> *stack) {
  bool res = false;
  auto &op1 = stack->end()[-2];
  const auto &str = op1.string();
  const auto &needle = stack->back().string();

  for (size_t i = 0; i + needle.size() <= str.size(); i++)
    if (str_caseeq(str.c_str() + i, needle.c_str(), needle.size())) {
      res = true;
      break;
    }

  op1 = rpn::Token(res);
  stack->pop_back();
}

const std::vector<rpn::Function_definition> functions = {
    {"SQRT", {NUMT}, NUMT, &reduce_sqrt},
    {"NUMBER", {STRT}, NUMT, &reduce_number},
    {"IS_IPV4", {STRT}, BOOLT, &reduce_is_ipv4},
    {"IS_IPV6", {STRT}, BOOLT, &reduce_is_ipv6},
    {"REGEXP_LIKE", {STRT, STRT}, BOOLT, &reduce_regexp_like},
    {"SUBSTRING_INDEX", {STRT, STRT, NUMT}, STRT, &reduce_substring_index},
    {"STARTSWITH", {STRT, STRT}, BOOLT, &reduce_str_startswith},
    {"ENDSWITH", {STRT, STRT}, BOOLT, &reduce_str_endswith},
    {"CONTAINS", {STRT, STRT}, BOOLT, &reduce_str_contains},
    // Those are going to be handled on their own
    {"RESOLVE_V4", {STRT}, STRT, nullptr},
    {"RESOLVE_V6", {STRT}, STRT, nullptr},
    {"CONCAT", {}, STRT, nullptr},
    {"NETWORK", {STRT, NUMT}, STRT, nullptr}};

const rpn::Function_definition *get_function_def(const std::string &name) {
  for (const auto &f : functions) {
    if (name == f.name) return &f;
  }
  return nullptr;
}

}  // namespace
}  // namespace routing_guidelines

namespace {

class Location {
 public:
  explicit Location(YYLTYPE *loc) : loc_(loc) {}

  void step() {
    loc_->first_line = loc_->last_line;
    loc_->first_column = loc_->last_column;
  }

  void lines(int lines) { loc_->last_line += lines; }

  Location &operator+=(int cols) {
    loc_->last_column += cols;
    return *this;
  }

  Location &operator-=(int cols) { return *this += -cols; }

 private:
  YYLTYPE *loc_;
};

std::string_view span_id(const std::string &s, std::string::size_type start,
                         bool *complex_id) {
  if (!std::isalpha(s[start]))
    throw std::runtime_error("Id not starting with a letter");
  *complex_id = false;
  auto i = start + 1;
  while (i < s.length()) {
    while (std::isalnum(s[i]) || s[i] == '_') ++i;
    if (s[i] != '.' || (!std::isalpha(s[i + 1]) && s[i + 1] != '_')) break;
    *complex_id = true;
    i += 2;
  }
  return {s.c_str() + start, i - start};
}

std::string::size_type span_num(const std::string &s,
                                std::string::size_type start, double *ret) {
  char *end = const_cast<char *>(&s[start]);
  *ret = strtod(end, &end);
  assert(end != &s[start]);
  return start + (end - &s[start]);
}

std::string::size_type span_quote(const std::string &buf,
                                  std::string::size_type offset) {
  char quote = buf[offset];
  for (size_t i = offset + 1; i < buf.length(); i++) {
    if (buf[i] == quote && buf[i - 1] != '\\') return i + 1;
  }
  throw std::runtime_error(std::string("unclosed ") + quote);
}

std::string::size_type span_brace(const std::string &buf,
                                  std::string::size_type offset) {
  char delim = buf[offset];
  char needle = delim == '{' ? '}' : ']';
  for (size_t i = offset + 1; i < buf.length(); i++) {
    if (buf[i] == needle) return i + 1;
  }
  throw std::runtime_error(std::string("unclosed ") + delim);
}

}  // namespace

static const std::unordered_map<std::string_view, int> keywords = {
    {"TRUE", T_TRUE}, {"IN", T_IN},       {"NOT", T_NOT},   {"AND", T_AND},
    {"OR", T_OR},     {"FALSE", T_FALSE}, {"NULL", T_NULL}, {"LIKE", T_LIKE}};

int yylex(union YYSTYPE *lvalp, YYLTYPE *llocp,
          routing_guidelines::Rules_parser *rp) {
  auto &buf = rp->buf_;
  auto &i = rp->pos_;
  auto loc = Location(llocp);
  bool complex_id = false;

  try {
    while (i < buf.length()) {
      loc.step();
      loc += 1;
      switch (buf[i++]) {
        case '\0':
          return T_END;
        case '-':
          return T_DASH;
        case '+':
          return T_PLUS;
        case '*':
          return T_STAR;
        case '/':
          return T_SLASH;
        case '%':
          return T_PERCENT;
        case '(':
          return T_LPAREN;
        case ')':
          return T_RPAREN;
        case '>': {
          if (buf[i] == '=') {
            ++i;
            loc += 1;
            return T_GE;
          }
          return T_GT;
        }
        case '<': {
          if (buf[i] == '=') {
            i++;
            loc += 1;
            return T_LE;
          } else if (buf[i] == '>') {
            i++;
            loc += 1;
            return T_NE;
          }
          return T_LT;
        }
        case '=':
          return T_EQ;
        case ',':
          return T_COMMA;

        case '$': {
          if (buf[i] != '.') {
            throw std::runtime_error("$ not starting variable reference");
          }
          lvalp->str = span_id(buf, i + 1, &complex_id);
          i += lvalp->str.length() + 1;
          loc += lvalp->str.length() + 1;
          return T_VARREF;
        }

        case '\'':
        case '"': {
          auto cb = span_quote(buf, i - 1);

          if (rp->context_->parse_tags_toggled()) {
            // be sure that the internal delimiter is always \"
            buf[i - 1] = '\"';
            buf[cb - 1] = '\"';
            // String tags should also contain delimiter
            lvalp->str = {buf.c_str() + i - 1, cb - i + 1};
          } else {
            lvalp->str = {buf.c_str() + i, cb - i - 1};
          }
          i = cb;
          loc += lvalp->str.length() + 1;
          return T_STRING;
        }

        case '{':
        case '[':
          if (rp->context_->parse_tags_toggled()) {
            auto cb = span_brace(buf, i - 1);
            lvalp->str = {buf.c_str() + i - 1, cb - i + 1};
            i = cb;
            loc += lvalp->str.length() + 1;
            return T_STRING;
          } else {
            loc -= 1;
            throw std::runtime_error(std::string("unexpected character: '") +
                                     buf[i - 1] + "'");
          }

        default:
          auto c = buf[i - 1];
          if (std::isspace(c)) continue;

          if (std::isdigit(c)) {
            auto start = i;
            double num = 0;
            i = span_num(buf, i - 1, &num);
            loc += i - start;

            if (rp->context_->parse_tags_toggled()) {
              lvalp->str = {buf.c_str() + start - 1, i - start + 1};
              return T_STRING;
            }

            lvalp->num = num;
            return T_NUMBER;
          }

          if (std::isalpha(c)) {
            auto sw = span_id(buf, i - 1, &complex_id);
            i += sw.length() - 1;
            loc += sw.length() - 1;
            lvalp->str = sw;
            if (!complex_id) {
              auto upid = routing_guidelines::str_upper(sw);
              auto kw = keywords.find(upid);
              if (kw != keywords.end()) {
                return rp->context_->parse_tags_toggled() ? T_STRING
                                                          : kw->second;
              }

              const auto func = routing_guidelines::get_function_def(upid);
              if (func != nullptr) {
                lvalp->func = func;
                return T_FUNCTION;
              }

              if (routing_guidelines::is_member_role(upid) ||
                  routing_guidelines::is_cluster_role(upid))
                return T_ROLE;
            }
            return T_IDENTIFIER;
          }

          loc -= 1;
          throw std::runtime_error(std::string("unexpected character: '") + c +
                                   "'");
      }
    }
  } catch (const std::exception &e) {
    yyerror(llocp, rp, (std::string("syntax error, ") + e.what()).c_str());
    return T_ERROR;
  }

  return T_END;
}

void yyerror(YYLTYPE *llocp, routing_guidelines::Rules_parser *rp,
             const char *msg) {
  if (!rp->error_.empty()) {
    // Bison adds this on unexpectedly finished input after the original error
    if (strstr(msg, "unexpected error") != nullptr) return;
    rp->error_ += ", ";
  }
  rp->error_ += routing_guidelines::rpn::error_msg(
      msg, rp->buf_, llocp->first_column, llocp->last_column);
}

namespace routing_guidelines {

rpn::Expression Rules_parser::parse(std::string buf, rpn::Context *context) {
  const auto scope_guard = context->start_parse_mode();
  rpn_.clear();
  error_.clear();
  buf_ = std::move(buf);
  pos_ = 0;
  context_ = context;
  if (yyparse(this)) {
    assert(!error_.empty());
    throw std::runtime_error(error_);
  }
  if (tracer_) {
    std::string msg{"Final expression: "};
    for (const auto &tok : rpn_) msg += to_string(tok, true) + " ";
    tracer_(msg);
  }
  return rpn::Expression(std::move(rpn_), std::move(buf_));
}

void Rules_parser::emit(rpn::Token::Type type, const YYLTYPE &loc) {
  rpn_.emplace_back(type, loc.first_column, loc.last_column);
  trace(to_string(rpn_.back()));
}

void Rules_parser::emit_null() {
  rpn_.emplace_back();
  trace(to_string(rpn_.back()));
}

void Rules_parser::emit_num(double val, rpn::Token::Type type) {
  rpn_.emplace_back(val, type);
  trace(to_string(rpn_.back(), true));
}

void Rules_parser::emit_string(std::string_view str, rpn::Token::Type type) {
  rpn_.emplace_back(mysql_unescape_string(str), type);
  trace(to_string(rpn_.back(), true));
}

void Rules_parser::emit_log_operation(rpn::Token::Type type, double arg_split,
                                      const YYLTYPE &loc) {
  rpn_.emplace(rpn_.end() - arg_split, arg_split + 1,
               type == rpn::Token::Type::OR ? rpn::Token::Type::MID_OR
                                            : rpn::Token::Type::MID_AND);
  rpn_.emplace_back(type, loc.first_column, loc.last_column);

  if (tracer_) {
    tracer_(to_string(rpn_.end()[-arg_split - 1], true));
    tracer_(to_string(rpn_.back(), true));
  }
}

bool Rules_parser::emit_in_op(const Exp_info &e, List_info *list,
                              YYLTYPE *llocp, Exp_info *ret) {
  Scope_guard guard([&]() {
    delete list;
    list = nullptr;
  });

  using Type = rpn::Token::Type;
  ret->type = Type::BOOL;
  ret->toks = e.toks + 1;
  for (size_t i = 0; i < list->size(); i++) {
    const auto elem = (*list)[i];
    if (e.type != Type::NONE && elem.type != Type::NONE &&
        type_error(llocp,
                   "in operator, type of element at offset " +
                       std::to_string(i) +
                       " does not match the type of searched element",
                   e.type, elem.type))
      return false;
    ret->toks += elem.toks;
  }
  if (list->size() > 1) {
    emit_num(list->size(), rpn::Token::Type::LIST);
    ++ret->toks;
  }
  emit(rpn::Token::Type::IN_OP, *llocp);
  return true;
}

bool Rules_parser::emit_like_op(const Exp_info &str, const Exp_info &pat,
                                YYLTYPE *llocp, Exp_info *ret) {
  if (type_error(llocp, "LIKE operator, left operand", STRT, str.type) ||
      type_error(llocp, "LIKE operator, right operand", STRT, pat.type))
    return false;

  if (!rpn_.back().is_string()) {
    yyerror(llocp, this,
            "LIKE operator only accepts string literals as its right operand");
    return false;
  }
  auto &pattern = rpn_.back().string();

  if (pattern.empty() || pattern == "%") {
    rpn_.resize(rpn_.size() - str.toks - pat.toks);
    rpn_.emplace_back(true);
    trace("Reducing with trivial LIKE operator");
    *ret = {1, BOOLT};
    return true;
  }

  // Remove escapes of the like pattern special characters
  const auto update_pattern = [&pattern](size_t start, size_t size) {
    if (size <= 1) return;
    std::string np;
    const size_t last = start + size - 1;
    for (size_t i = start; i < last; i++) {
      if (pattern[i] != '\\' ||
          (pattern[i + 1] != '%' && pattern[i + 1] != '_')) {
        np.push_back(pattern[i]);
      }
    }
    np.push_back(pattern[last]);
    pattern = std::move(np);
  };

  const auto args = [&pat, &str]() { return new List_info{str, pat}; };

  bool optimized = pattern[0] != '_' && pattern.back() != '_';
  for (size_t i = 1; optimized && i < pattern.size() - 1; i++)
    if ((pattern[i] == '%' || pattern[i] == '_') &&
        !(pattern[i - 1] == '\\' && (i < 2 || pattern[i - 2] != '\\')))
      optimized = false;

  if (optimized) {
    bool back_percent = pattern.back() == '%' &&
                        (pattern.end()[-2] != '\\' ||
                         (pattern.size() > 2 && pattern.end()[-3] == '\\'));

    if (pattern[0] == '%') {
      if (back_percent) {
        update_pattern(1, pattern.size() - 2);
        return emit_function(get_function_def("CONTAINS"), args(), llocp, ret);
      } else {
        update_pattern(1, pattern.size() - 1);
        return emit_function(get_function_def("ENDSWITH"), args(), llocp, ret);
      }
    } else if (back_percent) {
      update_pattern(0, pattern.size() - 1);
      return emit_function(get_function_def("STARTSWITH"), args(), llocp, ret);
    }
  }

  pattern = like_to_regexp(pattern);
  return emit_function(get_function_def("REGEXP_LIKE"), args(), llocp, ret);
}

bool Rules_parser::emit_function(const rpn::Function_definition *function,
                                 List_info *arguments, YYLTYPE *llocp,
                                 Exp_info *ret) {
  Scope_guard guard([&]() {
    delete arguments;
    arguments = nullptr;
  });

  assert(function != nullptr);
  std::string name{function->name};

  // Variable argument count function
  if (name == "CONCAT") return emit_concat(arguments, llocp, ret);

  int toks = 0;
  bool reducible = true;
  if (!arguments) {
    if (function->args.size() > 0) {
      std::string error{"syntax error, function "};
      error += name;
      error += " expected ";
      error += std::to_string(function->args.size());
      if (function->args.size() > 1) {
        error += " arguments but got none";
      } else {
        error += " argument but got none";
      }
      yyerror(llocp, this, error.c_str());
      return false;
    }
  } else {
    const auto &gargs = *arguments;
    const auto &eargs = function->args;
    if (eargs.size() != gargs.size()) {
      std::string error{"syntax error, function "};
      error += name;
      error += " expected ";
      error += std::to_string(eargs.size());
      if (eargs.size() > 1)
        error += " arguments but got ";
      else
        error += " argument but got ";
      error += std::to_string(gargs.size());
      yyerror(llocp, this, error.c_str());
      return false;
    }

    for (size_t i = 0; i < eargs.size(); i++) {
      if (eargs[i] != gargs[i].type) {
        std::string error{name + " function"};
        if (eargs.size() > 1) {
          error += ", ";
          error += std::to_string(i + 1);
          error += i == 0 ? "st" : i == 1 ? "nd" : i == 2 ? "rd" : "th";
          error += " argument";
        }
        if (type_error(llocp, error, eargs[i], gargs[i].type)) return false;
      }
      toks += gargs[i].toks;
      reducible = reducible && eargs[i] == rpn_.end()[i - eargs.size()].type();
    }
  }

  if (name == "RESOLVE_V4")
    return emit_resolve(llocp, ret, rpn::Token::Type::RESOLVE_V4);
  if (name == "RESOLVE_V6")
    return emit_resolve(llocp, ret, rpn::Token::Type::RESOLVE_V6);
  if (name == "NETWORK") return emit_network(ret);

  if (reducible) {
    try {
      function->reducer(&rpn_);
      trace("Reducing with " + name);
      *ret = {1, function->ret_val};
      assert(function->ret_val == rpn_.back().type());
      return true;
    } catch (const std::exception &e) {
      std::string error{"Function execution failed with error: "};
      error += e.what();
      yyerror(llocp, this, error.c_str());
      return false;
    }
  }

  if (name == "REGEXP_LIKE" && rpn_.back().is_string())
    return emit_regexp(arguments, llocp, ret);

  assert(function->reducer != nullptr);
  rpn_.emplace_back(*function, llocp->first_column, llocp->last_column);
  *ret = {toks + 1, function->ret_val};
  trace("'FUNCTION'(" + name + ")");

  return true;
}

bool Rules_parser::emit_network(Exp_info *ret) {
  auto val = rpn_.back().get_num();
  rpn_.back() = rpn::Token(val, rpn::Token::Type::NETWORK);
  *ret = {2, STRT};
  return true;
}

bool Rules_parser::emit_resolve(YYLTYPE *llocp, Exp_info *ret,
                                rpn::Token::Type resolve_ver) {
  std::string version_string =
      resolve_ver == rpn::Token::Type::RESOLVE_V4 ? "RESOLVE_V4" : "RESOLVE_V6";
  static const auto hostname_regex = std::regex(
      R"(^(([a-zA-Z0-9]|[a-zA-Z0-9][a-zA-Z0-9\-]*[a-zA-Z0-9])\.)*([A-Za-z0-9]|[A-Za-z0-9][A-Za-z0-9\-]*[A-Za-z0-9])$)");

  if (!rpn_.back().is_string()) {
    const auto &error =
        version_string +
        " function only accepts string literals as its parameter";
    yyerror(llocp, this, error.c_str());
    return false;
  }
  const auto &hostname = rpn_.back().string();
  if (!std::regex_match(hostname, hostname_regex)) {
    const auto error =
        version_string + " function, invalid hostname: '" + hostname + "'";
    yyerror(llocp, this, error.c_str());
    return false;
  }
  rpn_.back() = rpn::Token(str_lower(hostname), resolve_ver);
  *ret = {1, STRT};
  trace(to_string(rpn_.back(), true));
  return true;
}

bool Rules_parser::emit_concat(const List_info *arguments, YYLTYPE *llocp,
                               Exp_info *ret) {
  if (arguments == nullptr || arguments->empty()) {
    yyerror(llocp, this, "CONCAT function, no arguments provided");
    return false;
  }
  int toks = 1;
  for (const auto &a : *arguments) toks += a.toks;
  rpn_.emplace_back(arguments->size(), rpn::Token::Type::CONCAT);
  *ret = {toks, STRT};
  trace(to_string(rpn_.back(), true));
  return true;
}

bool Rules_parser::emit_regexp(const List_info *arguments, YYLTYPE *llocp,
                               Exp_info *ret) {
  try {
    auto &last = rpn_.back();
    last = rpn::Token::regexp(last.string());
    *ret = {arguments->front().toks + 1, BOOLT};
    trace(to_string(rpn_.back(), true));
  } catch (const std::exception &e) {
    std::string error{"REGEXP_LIKE function invalid regular expression: "};
    error += e.what();
    yyerror(llocp, this, error.c_str());
    return false;
  }
  return true;
}

void Rules_parser::is_extended_session_info(std::string_view name) {
  if (name == "session.user" || name == "session.schema" ||
      name.starts_with("session.connectAttrs")) {
    extended_session_info_in_use_ = true;
  }
}

void Rules_parser::uses_session_rand(std::string_view name) {
  if (name == "session.randomValue") session_rand_value_used_ = true;
}

bool Rules_parser::emit_reference(std::string_view name, YYLTYPE *llocp,
                                  Exp_info *ret) {
  is_extended_session_info(name);
  uses_session_rand(name);
  try {
    int offset{-1};
    *ret = {1, context_->get_type(std::string(name), &offset)};
    if (offset >= 0) {
      emit_num(offset, rpn::Token::Type::VAR_REF);
    } else {
      if (context_->get_version() > mysqlrouter::kBaseRoutingGuidelines &&
          (name.starts_with("router.tags") ||
           name.starts_with("server.tags"))) {
        context_->parsing_tags_ = true;
      }
      emit_string(name, rpn::Token::Type::TAG_REF);
    }
    return true;
  } catch (...) {
  }
  yyerror(llocp, this, ("undefined variable: " + std::string(name)).c_str());
  return false;
}

bool Rules_parser::check_role_types(const Exp_info &left, const Exp_info &right,
                                    YYLTYPE *llocp) {
  using Type = rpn::Token::Type;
  // no check required
  if (left.type != Type::ROLE || right.type != Type::ROLE) return false;

  // Roles are not a result of any computations
  assert(left.toks == 1 && right.toks == 1);

  enum Role_type { BOTH, MEMBER, CLUSTER, UNDEFINED };
  int member_role_id{-1};
  context_->get_type("server.memberRole", &member_role_id);
  int cluster_role_id{-1};
  context_->get_type("server.clusterRole", &cluster_role_id);

  const auto get_role_type = [&](const rpn::Token &t) -> Role_type {
    std::string s;
    if (t.type() == Type::ROLE) {
      s = t.string();
    } else if (t.type() == Type::VAR_REF) {
      if (t.number() == cluster_role_id) return Role_type::CLUSTER;
      if (t.number() == member_role_id) return Role_type::MEMBER;
    } else
      assert(false);

    if (routing_guidelines::is_member_role(s))
      return routing_guidelines::is_cluster_role(s) ? BOTH : MEMBER;
    assert(routing_guidelines::is_cluster_role(s));
    return CLUSTER;
  };

  const Role_type left_role = get_role_type(rpn_.end()[-2]);
  if (left_role == BOTH) return false;

  const auto right_role = get_role_type(rpn_.back());
  if (right_role == BOTH || right_role == left_role) return false;

  if (right_role == UNDEFINED || left_role == UNDEFINED) return false;

  if (left_role == MEMBER)
    yyerror(llocp, this,
            "type error, incompatible operands for comparison: 'MEMBER "
            "ROLE' vs 'CLUSTER ROLE'");
  else
    yyerror(llocp, this,
            "type error, incompatible operands for comparison: 'CLUSTER "
            "ROLE' vs 'MEMBER ROLE'");

  return true;
}

bool Rules_parser::type_error(YYLTYPE *llocp, const std::string &msg,
                              rpn::Token::Type expected, rpn::Token::Type got) {
  if (expected == got) return false;
  std::string error("type error, ");
  error += msg;
  error += ", expected ";
  error += rpn::to_string(expected);
  error += " but got ";
  error += rpn::to_string(got);
  yyerror(llocp, this, error.c_str());
  return true;
}

std::vector<std::string> Rules_parser::get_keyword_names() {
  std::vector<std::string> result;
  std::transform(std::begin(keywords), std::end(keywords),
                 std::back_inserter(result),
                 [](const auto &key) { return std::string(key.first); });
  return result;
}

std::vector<std::string> Rules_parser::get_function_names() {
  std::vector<std::string> result;
  std::transform(std::begin(functions), std::end(functions),
                 std::back_inserter(result),
                 [](const auto &function) { return function.name; });
  return result;
}

}  // namespace routing_guidelines
