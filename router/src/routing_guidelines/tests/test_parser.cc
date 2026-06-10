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

#include <gtest/gtest.h>

#ifdef _WIN32
#include <WinSock2.h>
#include <windows.h>
#endif

#include "helpers/router_test_helpers.h"  // EXPECT_THROW_LIKE
#include "routing_guidelines/routing_guidelines.h"
#include "rules_parser.h"
#include "utils.h"

#define EXPECT_PE(a, b) EXPECT_THROW_LIKE(parse(a), std::runtime_error, b)
#define EXPECT_EE(a, b) EXPECT_THROW_LIKE(parse_eval(a), std::runtime_error, b)

namespace routing_guidelines {

using Tk = rpn::Token;

class Routing_guidelines_parser_test : public ::testing::Test {
 protected:
  rpn::Expression parse(const std::string &code) {
    return rp.parse(code, &ctx);
  }

  rpn::Token parse_eval(const std::string &code) {
    return rp.parse(code, &ctx).eval(&ctx, &cache);
  }

  void EXPECT_NUM(double num, const std::string &exp) {
    SCOPED_TRACE(exp);
    try {
      auto res = parse_eval(exp);
      ASSERT_TRUE(res.is_num());
      EXPECT_EQ(num, res.number());
    } catch (const std::exception &e) {
      ADD_FAILURE() << "Unexpected exception: " << e.what() << "\n";
    }
  }

  void EXPECT_ROLE(const std::string &role, const std::string &exp) {
    SCOPED_TRACE(exp);
    try {
      auto res = parse_eval(exp);
      EXPECT_EQ(rpn::Token(role, rpn::Token::Type::ROLE), res);
    } catch (const std::exception &e) {
      ADD_FAILURE() << "Unexpected exception: " << e.what() << "\n";
    }
  }

  void EXPECT_T(const std::string &exp) {
    SCOPED_TRACE(exp);
    try {
      EXPECT_TRUE(parse_eval(exp).get_bool());
    } catch (const std::exception &e) {
      ADD_FAILURE() << "Unexpected exception: " << e.what() << "\n";
    }
  }

  void EXPECT_F(const std::string &exp) {
    SCOPED_TRACE(exp);
    try {
      EXPECT_FALSE(parse_eval(exp).get_bool());
    } catch (const std::exception &e) {
      ADD_FAILURE() << "Unexpected exception: " << e.what() << "\n";
    }
  }

  void EXPECT_STR(const std::string &s, const std::string &exp) {
    SCOPED_TRACE(exp);
    try {
      auto res = parse_eval(exp);
      ASSERT_TRUE(res.is_string());
      EXPECT_EQ(s, res.string());
    } catch (const std::exception &e) {
      ADD_FAILURE() << "Unexpected exception: " << e.what() << "\n";
    }
  }

  void EXPECT_NULL(const std::string &exp) {
    SCOPED_TRACE(exp);
    try {
      EXPECT_TRUE(parse_eval(exp).is_null());
    } catch (const std::exception &e) {
      ADD_FAILURE() << "Unexpected exception: " << e.what() << "\n";
    }
  }

  void EXPECT_OPT(const std::string &code, const std::string &optimization,
                  bool res) {
    SCOPED_TRACE(code);
    try {
      auto exp = parse(code);
      if (optimization == "REGEXP_LIKE") {
        EXPECT_EQ(rpn::Token::Type::REGEXP, exp.rpn_.back().type());
      } else {
        EXPECT_EQ(optimization, exp.rpn_.back().function().name);
      }
      EXPECT_EQ(res, exp.eval(&ctx).get_bool());
    } catch (const std::exception &e) {
      ADD_FAILURE() << "Unexpected exception: " << e.what() << "\n";
    }
  }

  Rules_parser rp;
  rpn::Context ctx;
  Routing_guidelines_engine::ResolveCache cache;
};

TEST_F(Routing_guidelines_parser_test, context_variable_wrapping) {
  EXPECT_THROW_LIKE(parse_eval("$.server.port"), std::runtime_error,
                    "server.port");
  {
    Server_info si{"NumberOne",
                   "127.0.0.1",
                   3306,
                   33060,
                   "123e4567-e89b-12d3-a456-426614174000",
                   80023,
                   "SECONDARY",
                   {{"uptime", "2 years"}, {"alarm", "9PM"}},
                   "Unnamed",
                   "Set1",
                   "",
                   false};
    ctx.set_server_info(si);
    EXPECT_STR(si.label, "$.server.label");
    EXPECT_STR(si.address, "$.server.address");
    EXPECT_NUM(si.port, "$.server.port");
    EXPECT_STR(si.uuid, "$.server.uuid");
    EXPECT_NUM(si.version, "$.server.version");
    EXPECT_ROLE(si.member_role, "$.server.memberRole");
    EXPECT_STR(si.tags["uptime"], "$.server.tags.uptime");
    EXPECT_STR(si.tags["alarm"], "$.server.tags.alarm");
    EXPECT_STR(si.cluster_name, "$.server.clusterName");
    EXPECT_STR(si.cluster_set_name, "$.server.clusterSetName");
    EXPECT_ROLE(kUndefinedRole, "$.server.clusterRole");
  }

  ctx.clear_server_info();
  EXPECT_THROW_LIKE(parse_eval("$.server.port"), std::runtime_error,
                    "server.port");

  EXPECT_THROW_LIKE(parse_eval("$.session.port"), std::runtime_error,
                    "session.port");
  {
    Session_info si{"196.0.0.1",
                    3306,
                    "123.222.111.12",
                    "root",
                    {{"uptime", "2 years"}, {"alarm", "9PM"}},
                    "test",
                    1};
    ctx.set_session_info(si);
    EXPECT_STR(si.target_ip, "$.session.targetIP");
    EXPECT_NUM(si.target_port, "$.session.targetPort");
    EXPECT_STR(si.source_ip, "$.session.sourceIP");
    EXPECT_STR(si.user, "$.session.user");
    EXPECT_STR(si.connect_attrs["uptime"], "$.session.connectAttrs.uptime");
    EXPECT_STR(si.connect_attrs["alarm"], "$.session.connectAttrs.alarm");
    EXPECT_STR(si.schema, "$.session.schema");
  }
  ctx.clear_session_info();
  EXPECT_THROW_LIKE(parse_eval("$.session.port"), std::runtime_error,
                    "session.port");

  EXPECT_THROW_LIKE(parse_eval("$.router.port"), std::runtime_error,
                    "router.port");
  Router_info ri{3306,
                 3307,
                 3308,
                 "Cluster0",
                 "mysql.oracle.com",
                 "192.168.0.123",
                 {{"uptime", "2 years"}, {"alarm", "9PM"}},
                 "routing_ro",
                 "test-router"};
  ctx.set_router_info(ri);
  EXPECT_NUM(ri.port_ro, "$.router.port.ro");
  EXPECT_NUM(ri.port_rw, "$.router.port.rw");
  EXPECT_NUM(ri.port_rw_split, "$.router.port.rw_split");
  EXPECT_STR(ri.local_cluster, "$.router.localCluster");
  EXPECT_STR(ri.hostname, "$.router.hostname");
  EXPECT_STR(ri.bind_address, "$.router.bindAddress");
  EXPECT_STR(ri.tags["uptime"], "$.router.tags.uptime");
  EXPECT_STR(ri.tags["alarm"], "$.router.tags.alarm");
  ctx.set_router_info(Router_info{7306,
                                  3307,
                                  3308,
                                  "Cluster1",
                                  "mysql.oracle.com",
                                  "192.168.5.123",
                                  {},
                                  "routing_foo",
                                  "test-router"});
  EXPECT_THROW_LIKE(parse_eval("$.router.port"), std::runtime_error,
                    "router.port");
}

TEST_F(Routing_guidelines_parser_test, numerical_expressions) {
  ctx.set("a", 10.0);

  EXPECT_NUM(10, "10.0");
  EXPECT_NUM(10, "10");
  EXPECT_NUM(-10, "-10.0");
  EXPECT_NUM(-10, "-10");
  EXPECT_NUM(10, "$.a");
  EXPECT_NUM(-10, "-$.a");

  EXPECT_NUM(20, "10 + $.a");
  EXPECT_NUM(0, "10 - $.a");
  EXPECT_NUM(15, "1.5*$.a");
  EXPECT_NUM(0.5, "$.a/20");
  EXPECT_NUM(10, "$.a % 11");

  EXPECT_NUM(22, "10 + 3*4");
  EXPECT_NUM(26, "(10 + 3)*2");
  EXPECT_NUM(0, "12 - 3*4");
  EXPECT_NUM(14, "(10 - 3)*2");
  EXPECT_NUM(12, "10 + 8/4");
  EXPECT_NUM(3, "(2 + 4)/2");
  EXPECT_NUM(11.5, "12 - 4/8");
  EXPECT_NUM(3, "(10 - 4)/2");
  EXPECT_NUM(2, "(10 - 8) % 3");
  EXPECT_NUM(2, "(10 + 8)%4");
  EXPECT_NUM(9, "12 - 3%4");
  EXPECT_NUM(14, "12 + 6%4");

  EXPECT_NUM(22, "$.a + 3*4");
  EXPECT_NUM(26, "($.a + 3)*2");
  EXPECT_NUM(-2, "$.a - 3*4");
  EXPECT_NUM(14, "($.a - 3)*2");
  EXPECT_NUM(12, "$.a + 8/4");
  EXPECT_NUM(-7, "-($.a + 4)/2");
  EXPECT_NUM(9.5, "$.a - 4/8");
  EXPECT_NUM(3, "($.a - 4)/2");
  EXPECT_NUM(2, "($.a - 8) % 3");
  EXPECT_NUM(2, "($.a + 8)%4");
  EXPECT_NUM(7, "$.a - 3%4");
  EXPECT_NUM(12, "$.a + 6%4");
  EXPECT_NUM(9, "sqrt(81)");
  EXPECT_NUM(11, "1 + sqrt(10*9 + $.a)");

  EXPECT_NUM(63, "123/3*4-78*2+56%70-6*6/3%11");
  EXPECT_NUM(-635344.2,
             "-(11*12/6*8%43 + 65.5 * 78.8) * (124 - sqrt($.a *2.5) % 4)");
  EXPECT_NUM(63, "$.a*12.3/3*4-78*sqrt(4)+56%70-6*6/3%11");
  EXPECT_NUM(-635344.2,
             "-(($.a+1)*12/6*8%43 + 65.5 * 78.8) * (124 - sqrt(25) % 4)");

  // Converting string to number
  EXPECT_NUM(10.123, "number('10.123')");
  EXPECT_NUM(-10, "number('-10')");
  EXPECT_NUM(112, "number('112')");
  EXPECT_NUM(-123.123, "number('-123.123')");

  // Empty string converts to 0
  EXPECT_NUM(0, "number('')");
  EXPECT_PE("number('17a')",
            "NUMBER function, unable to convert '17a' to number");

  ctx.set("right", "777");
  ctx.set("wrong", "77a");
  EXPECT_NUM(777, "number($.right)");
  EXPECT_EE("number($.wrong)",
            "NUMBER function, unable to convert '77a' to number");
}

TEST_F(Routing_guidelines_parser_test, strings) {
  ctx.set("mysql", "MySQL");
  ctx.set("microsoft", "SQL Server");
  ctx.set("web", "www.mysql.com");

  EXPECT_STR("Windows XP", "'Windows XP'");
  EXPECT_STR("Windows XP", "\"Windows XP\"");
  EXPECT_STR("Windows7", "Windows7");
  EXPECT_STR("MySQL", "$.mysql");
  EXPECT_STR("SQL Server", "$.microsoft");

  // String escapes
  EXPECT_STR("a\bcdefghijklm\nopq\rs\tuvwxyz",
             R"('\a\b\c\d\e\f\g\h\i\j\k\l\m\n\o\p\q\r\s\t\u\v\w\x\y\z')");
  EXPECT_STR("ABCDEFGHIJKLMNOPQRSTUVWXY\032",
             R"('\A\B\C\D\E\F\G\H\I\J\K\L\M\N\O\P\Q\R\S\T\U\V\W\X\Y\Z')");
  std::string res{"\\123456789"};
  res.push_back('\0');
  EXPECT_STR(res, R"('\\\1\2\3\4\5\6\7\8\9\0')");

  EXPECT_T("REGEXP_LIKE('PostgreSQL', '.*SQL')");
  EXPECT_F("REGEXP_LIKE($.microsoft, '.*SQL')");
  EXPECT_T("REGEXP_LIKE($.microsoft, 'SQL.*')");
  EXPECT_PE("regexp_like($.web, '[a-b][a')",
            "REGEXP_LIKE function invalid regular expression");

  EXPECT_STR("", "SUBSTRING_INDEX('www.mysql.com', '.', 0)");
  EXPECT_STR("", "SUBSTRING_INDEX($.web, '.', 0)");

  EXPECT_STR("www.mysql.com", "SUBSTRING_INDEX('www.mysql.com', '.', 3)");
  EXPECT_STR("www.mysql.com", "SUBSTRING_INDEX($.web, '.', 3)");
  EXPECT_STR("www.mysql", "SUBSTRING_INDEX('www.mysql.com', '.', 2)");
  EXPECT_STR("www.mysql", "SUBSTRING_INDEX($.web, '.', 2)");
  EXPECT_STR("www", "SUBSTRING_INDEX('www.mysql.com', '.', 1)");
  EXPECT_STR("www", "SUBSTRING_INDEX($.web, '.', 1)");
  EXPECT_STR("www.mysql.com", "SUBSTRING_INDEX('www.mysql.com', '.', 20)");
  EXPECT_STR("www.mysql.com", "SUBSTRING_INDEX($.web, '.', 20)");

  // When delimiter not found return whole string
  EXPECT_STR("www.mysql.com", "SUBSTRING_INDEX('www.mysql.com', ',', 1)");
  EXPECT_STR("www.mysql.com", "SUBSTRING_INDEX($.web, ',', -1)");

  EXPECT_STR("www.mysql.com", "SUBSTRING_INDEX('www.mysql.com', '.', -3)");
  EXPECT_STR("www.mysql.com", "SUBSTRING_INDEX($.web, '.', -3)");
  EXPECT_STR("mysql.com", "SUBSTRING_INDEX('www.mysql.com', '.', -2)");
  EXPECT_STR("mysql.com", "SUBSTRING_INDEX($.web, '.', -2)");
  EXPECT_STR("com", "SUBSTRING_INDEX('www.mysql.com', '.', -1)");
  EXPECT_STR("com", "SUBSTRING_INDEX($.web, '.', -1)");
  EXPECT_STR("www.mysql.com", "SUBSTRING_INDEX('www.mysql.com', '.', -20)");
  EXPECT_STR("www.mysql.com", "SUBSTRING_INDEX($.web, '.', -20)");

  EXPECT_T("STARTSWITH('www.mysql.com', 'www.mysql')");
  EXPECT_T("STARTSWITH($.web, 'www.')");
  EXPECT_T("STARTSWITH('www.Mysql.com', 'Www.mysql')");
  EXPECT_T("STARTSWITH($.web, 'wWw.')");
  EXPECT_F("STARTSWITH('www.mysql.com', 'www,')");
  EXPECT_F("STARTSWITH($.web, 'mysql')");

  EXPECT_T("ENDSWITH('www.mysql.com', 'mysql.com')");
  EXPECT_T("ENDSWITH($.web, '.com')");
  EXPECT_T("ENDSWITH('www.Mysql.Com', 'mysqL.com')");
  EXPECT_T("ENDSWITH($.web, 'COM')");
  EXPECT_F("ENDSWITH('www.mysql.com', '.con')");
  EXPECT_F("ENDSWITH('com', 'www.mysql.com')");
  EXPECT_F("ENDSWITH($.web, '.org')");
  EXPECT_F("ENDSWITH('.com', $.web)");

  EXPECT_T("CONTAINS('www.mysql.com.pl', 'mysql.COM')");
  EXPECT_T("CONTAINS('www.mysql.com', 'mysql.com')");
  EXPECT_T("CONTAINS('www.mysql.com', 'WWW')");
  EXPECT_T("CONTAINS($.web, '.Com')");
  EXPECT_T("CONTAINS('www.Mysql.Com', $.web)");
  EXPECT_T("CONTAINS($.web, 'w.M')");
  EXPECT_T("CONTAINS($.web, '')");
  EXPECT_F("CONTAINS($.web, 'www.mysql.com1')");
  EXPECT_F("CONTAINS('www.mysql.org', $.web)");
  EXPECT_F("CONTAINS('', $.web)");

  // CONCAT function
  EXPECT_NULL("CONCAT (NULL)");
  EXPECT_NULL("CONCAT(1, NULL)");
  EXPECT_NULL("CONCAT(NULL, 'ele')");
  EXPECT_NULL("CONCAT(1, 'ele', NULL)");

  EXPECT_STR("abra", "concat('abra')");
  EXPECT_STR("abracadabra,elemele", "concat(abra, cadabra, ',', ele, 'mele')");

  // roles are strings too
  EXPECT_STR("primary,REPLICA", "concat(primary, ',', REPLICA)");

  EXPECT_STR("1", "concat (true)");
  EXPECT_STR("0", "concat(false)");
  EXPECT_STR("777", "concat (777)");
  EXPECT_STR("777.777", "CONCAT (777.777)");
  EXPECT_STR("abra1123.123cadabra1230",
             "concat('abra', TRUE, 123.123, cadabra, 123, FALSE)");
  ctx.set("float", 123.123);
  ctx.set("bool", true);
  ctx.set("str", "abra");
  EXPECT_STR("abra1123.123cadabra1230",
             "concat($.str, $.bool, $.float, \"cadabra\", 123, FALSE)");

  EXPECT_PE("concat()", "CONCAT function, no arguments provided");
}

TEST_F(Routing_guidelines_parser_test, like_operator) {
  ctx.set("web", "www.mysql.com");
  ctx.set("fun", "%_%_%");

  EXPECT_T("$.web like ''");
  EXPECT_T("$.fun like '%'");

  EXPECT_OPT("$.web LIKE '%mysql%'", "CONTAINS", true);
  EXPECT_OPT("$.web LIKE '%mysql.com'", "ENDSWITH", true);
  EXPECT_OPT("$.web LIKE 'www.%'", "STARTSWITH", true);
  EXPECT_OPT(R"($.fun LIKE '\\%\\_%')", "STARTSWITH", true);
  EXPECT_OPT(R"($.fun LIKE '%\\_\\%')", "ENDSWITH", true);

  EXPECT_EQ(R"(.\*.* \^ \$ \\ \. \* \+ \? \( \) \[ \] \{ \} \|..*)",
            like_to_regexp(R"(_*% ^ $ \ . * + ? ( ) [ ] { } |_%)"));
  EXPECT_EQ(".*..*_%.%_.", like_to_regexp(R"(%_%\_\%_\%\__)"));
  EXPECT_EQ(R"(\\d\\sds\\D\\S\\W\\w.*)", like_to_regexp(R"(\d\sds\D\S\W\w%)"));

  EXPECT_OPT("$.web LIKE '___.%.___'", "REGEXP_LIKE", true);
  EXPECT_F("$.web not LIKE '___.%.___'");
  EXPECT_OPT("$.web LIKE '___.%.___'", "REGEXP_LIKE", true);
  EXPECT_F("$.web NOT like '___.%.___'");
  EXPECT_OPT("$.fun LIKE '%\\__\\_\\%'", "REGEXP_LIKE", true);
  EXPECT_F("$.fun NOT LIKE '%\\__\\_\\%'");
  EXPECT_OPT("$.fun LIKE '.*'", "REGEXP_LIKE", false);
  EXPECT_T("$.fun not like '.*'");

  EXPECT_PE("'abradab' LIKE $.fun",
            "LIKE operator only accepts string literals as its right operand");
}

TEST_F(Routing_guidelines_parser_test, server_roles) {
  Server_info si{"NumberOne",
                 "127.0.0.1",
                 3306,
                 33060,
                 "123e4567-e89b-12d3-a456-426614174000",
                 80023,
                 "SECONDARY",
                 {{"uptime", "2 years"}, {"alarm", "9PM"}},
                 "Unnamed",
                 "Set1",
                 "REPLICA",
                 false};
  ctx.set_server_info(si);

  EXPECT_ROLE(kUndefinedRole, "UNDEFINED");
  EXPECT_ROLE("PRIMARY", "PRIMARY");
  EXPECT_ROLE("SECONDARY", "SECONDARY");
  EXPECT_ROLE("REPLICA", "REPLICA");
  EXPECT_ROLE("REPLICA", "$.server.clusterRole");
  EXPECT_ROLE("SECONDARY", "$.server.memberRole");

  EXPECT_F(kUndefinedRole);
  EXPECT_T("PRIMARY");
  EXPECT_T("SECONDARY");
  EXPECT_T("REPLICA");

  EXPECT_F("PRIMARY = SECONDARY");
  EXPECT_T("PRIMARY <> secondary");
  EXPECT_F("UNDEFINED = SECONDARY");
  EXPECT_T("PRIMARY = primary");
  EXPECT_F("UNDEFINED = PRIMARY");
  EXPECT_T("SECONDARY <> PRIMARY");
  EXPECT_F("REPLICA = UNDEFINED");

  EXPECT_T("$.server.clusterRole");
  EXPECT_T("REPLICA = $.server.clusterRole");
  EXPECT_F("$.server.memberRole = PRIMARY");
  EXPECT_T("$.server.memberRole = SECONDARY");
  EXPECT_F("$.server.clusterRole = PRIMARY");
  EXPECT_T("$.server.memberRole <> UNDEFINED");

  EXPECT_PE("$.server.clusterRole = SECONDARY",
            "type error, incompatible operands for comparison: "
            "'CLUSTER ROLE' vs 'MEMBER ROLE'");
  EXPECT_PE("SECONDARY <> $.server.clusterRole",
            "type error, incompatible operands for comparison: "
            "'MEMBER ROLE' vs 'CLUSTER ROLE'");
  EXPECT_PE("$.server.memberRole <> $.server.clusterRole",
            "type error, incompatible operands for comparison: "
            "'MEMBER ROLE' vs 'CLUSTER ROLE'");
  EXPECT_PE("$.server.memberRole = 'undefined'",
            "the type of left operand does not match right, expected "
            "ROLE but got STRING");
  EXPECT_PE("$.server.clusterRole <> 0",
            "left operand does not match right, expected ROLE but got NUMBER");
}

TEST_F(Routing_guidelines_parser_test, null_values) {
  Session_info si;

  const auto eval_missing = [&](const std::string &exp) {
    rpn::Expression parsed;
    ctx.set_session_info(si);
    parsed = parse(exp);
    ctx.clear_session_info();
    return parsed.eval(&ctx);
  };

  ctx.set("null_variable", rpn::Token());
  EXPECT_NULL("null");
  EXPECT_NULL("NULL");
  EXPECT_NULL("$.null_variable");
  EXPECT_TRUE(eval_missing("$.session.targetPort").is_null());
  EXPECT_TRUE(eval_missing("$.session.user").is_null());

  // arythmetic operation return null when one of the arguments is null but only
  // accept nulls as a result of missing variable
  EXPECT_TRUE(eval_missing("$.session.targetPort + 2").is_null());
  EXPECT_TRUE(eval_missing("2 - $.session.targetPort").is_null());
  EXPECT_TRUE(eval_missing("$.session.targetPort * 5").is_null());
  EXPECT_TRUE(eval_missing("6 / $.session.targetPort").is_null());
  EXPECT_TRUE(eval_missing("$.session.targetPort + 2").is_null());
  EXPECT_TRUE(eval_missing("4 % $.session.targetPort").is_null());
  EXPECT_TRUE(eval_missing("sqrt($.session.targetPort)").is_null());
  EXPECT_PE("NULL + 2", "type error, + operator, left operand");
  EXPECT_PE("1 % NULL", "type error, % operator, right operand");

  // comparisons other than '=' nad '<>' return type error for values of type
  // null, evaluate to false if null is a result of missing variable value
  EXPECT_F("null = 3");
  EXPECT_F("'string' = $.null_variable");
  EXPECT_T("$.null_variable = null");
  EXPECT_F("NULL <> $.null_variable");
  EXPECT_TRUE(eval_missing("sqrt($.session.targetPort) = NULL").get_bool());
  EXPECT_FALSE(eval_missing("NULL <> sqrt($.session.targetPort)").get_bool());
  EXPECT_FALSE(eval_missing("$.session.user < 'abradab'").get_bool());
  EXPECT_FALSE(eval_missing("'abradab' >= $.session.user").get_bool());
  EXPECT_PE("NULL > 2", "NULL type arguments cannot be compared");
  EXPECT_PE("'abra' <= NULL", "the type of left operand does not match right");

  // inside IN expressions NULLs are evaluated according to equality comparison
  // rules
  EXPECT_T("1 in (2, null, 2-1)");
  EXPECT_F("NULL in (2, 3, 2-1)");
  EXPECT_T("null in ('dwa', trzy, $.null_variable)");
  EXPECT_F("$.null_variable in ('ene', 'due')");
  EXPECT_T("null in ($.null_variable)");
  EXPECT_TRUE(eval_missing("$.session.user in ('root', NULL)").get_bool());

  // in logical operation NULL evaluates to false
  EXPECT_T("NOT null");
  EXPECT_F("TRUE AND NULL");
  EXPECT_T("FALSE OR NOT NULL");
  EXPECT_F("null or false");

  // functions only accept NULL values as a result of missing variable
  EXPECT_TRUE(eval_missing("REGEXP_LIKE($.session.user, 'SQL.*')").is_null());
  EXPECT_PE("REGEXP_LIKE('MySQL', NULL)", "got NULL");

  // with exception of resolve, which needs string literals
  EXPECT_THROW_LIKE(
      eval_missing("RESOLVE_V4($.session.user)"), std::runtime_error,
      "RESOLVE_V4 function only accepts string literals as its parameter");
  EXPECT_THROW_LIKE(
      eval_missing("RESOLVE_V6($.session.user)"), std::runtime_error,
      "RESOLVE_V6 function only accepts string literals as its parameter");

  EXPECT_PE("RESOLVE_V4(NULL)", "got NULL");
  EXPECT_PE("RESOLVE_V6(NULL)", "got NULL");
}

TEST_F(Routing_guidelines_parser_test, comparisons) {
  ctx.set("a", 10.0);
  ctx.set("mysql", "MySQL");
  ctx.set("postgres", "Postgres");

  EXPECT_T("10 < 11");
  EXPECT_T("10 <= 11");
  EXPECT_T("10 <= 10");
  EXPECT_F("10 < 9");
  EXPECT_F("10 <= 9");
  EXPECT_T("$.a < 11");
  EXPECT_T("$.a <= 11");
  EXPECT_T("$.a <= 10");
  EXPECT_F("$.a < 9");
  EXPECT_F("$.a <= 9");

  EXPECT_T("11 > 10");
  EXPECT_T("11 >= 10");
  EXPECT_T("10 >= 10");
  EXPECT_F("8 > 9");
  EXPECT_F("8 >= 9");
  EXPECT_T("11 > $.a");
  EXPECT_T("11 >= $.a");
  EXPECT_T("10 >= $.a");
  EXPECT_F("9 > $.a");
  EXPECT_F("9 >= $.a");

  EXPECT_F("11 -1 = 10*0.5");
  EXPECT_T("11 /2 <> 10 *2");
  EXPECT_T("10 = 10");
  EXPECT_F("8 <> 8");
  EXPECT_T("10 = $.a");
  EXPECT_F("$.a <> $.a");
  EXPECT_F("$.a * 3 <> 30 % 31");
  EXPECT_T("4 * $.a = ($.a + $.a) * 2");
  EXPECT_F("$.a = $.a + 1");

  EXPECT_T("'MySQL' = mysql");
  EXPECT_T("MySQL = \"mysql\"");
  EXPECT_F("'MySQL' <> mysql");
  EXPECT_F("MySQL <> \"mysql\"");
  EXPECT_F("'Postgres' = mysql");
  EXPECT_T("postgres <> \"mysql\"");
  EXPECT_F("$.mysql = $.postgres");
  EXPECT_T("$.mysql = $.mysql");
  EXPECT_T("$.mysql <> $.postgres");
  EXPECT_T("POSTGRES = $.postgres");
  EXPECT_T("$.mysql = mysql");

  EXPECT_T("Anna < Maria");
  EXPECT_F("'Maria' <= \"Anna\"");
  EXPECT_T("Anna < $.mysql");
  EXPECT_T("$.postgres > $.mysql");
  EXPECT_T("$.postgres >= $.mysql");
  EXPECT_T("Mongo < $.mysql");
  EXPECT_T("mongo <= $.mysql");
  EXPECT_T("mongo <= MONGO");
}

TEST_F(Routing_guidelines_parser_test, logical_operations) {
  ctx.set("a", 10.0);
  ctx.set("mysql", "MySQL");
  ctx.set("t", true);
  ctx.set("f", false);

  EXPECT_T("true");
  EXPECT_F("False");
  EXPECT_F("NOT TRUe");
  EXPECT_T("NOT falsE");

  EXPECT_T("true or false");
  EXPECT_F("true and false");
  EXPECT_T("$.f or true");
  EXPECT_F("false and $.t");
  EXPECT_F("false or false");
  EXPECT_F("false and $.f");
  EXPECT_T("$.t or true");
  EXPECT_T("true and true");

  EXPECT_T("'' or 'stg'");
  EXPECT_F("'' and 'stg'");
  EXPECT_T("0 or 11");
  EXPECT_F("2.2 and 0");
  EXPECT_T("'' or 1");
  EXPECT_F("0 and 'stg'");

  EXPECT_F("NOT (false or true)");
  EXPECT_T("NOT $.f and $.t");
  EXPECT_T("NOT ''");
  EXPECT_F("NOT 'stg'");
  EXPECT_T("NOT 0");
  EXPECT_F("NOT 1.1");
  EXPECT_F("NOT 7");

  EXPECT_T("2+2 > 2-2 AND Abba < Beatles");
  EXPECT_T("2/2 < 2%4 AND NOT Abba >= Beatles");
  EXPECT_F("2*2 <= 2%4 OR Abba >= $.mysql");
  EXPECT_T("$.a/2 <= 2%4 OR Abba <= Beatles");

  // Conditional execution of second part of the logical expression
  ctx.set("wrong_address", "matata");
  ctx.set("postgres", rpn::Token());

  EXPECT_T("true or $.a = 1 or network($.wrong_address, 16)");
  EXPECT_F("false and 9 = $.a/0 and network($.wrong_address, 16)");
  EXPECT_T(
      "true or resolve_v4('oracle.com') > '123' or $.a in (1, 2, "
      "sqrt($.a) % 2, -37.5) or UNDEFINED NOT IN (REPLICA)");
  EXPECT_F(
      "false and NOT regexp_like($.mysql, '(sub)(.*)') and $.a not in ($.a - "
      "10, NULL) and $.a > sqrt($.a)");
  EXPECT_T(
      "-$.a = -10 or network($.wrong_address, 12) <= '127.0.0.1' or -$.a >= 12 "
      "* 3 or $.a/0 <> null or false or contains($.wrong_address, "
      "'hakuna')");
  EXPECT_T(
      "$.a <> 10 and $.postgres in (REPLICA, SECONDARY) or $.a * 7 > 3 or 12 "
      "% -$.a > 777");
}

TEST_F(Routing_guidelines_parser_test, in_operator) {
  ctx.set("a", "a");
  ctx.set("mysql", "MySQL");
  ctx.set("postgres", "Postgres");

  EXPECT_T("a in (a)");
  EXPECT_THROW_LIKE(parse("$.a in a"), std::runtime_error,
                    "syntax error, unexpected identifier, expecting (");
  EXPECT_THROW_LIKE(parse("$.a not in a"), std::runtime_error,
                    "syntax error, unexpected identifier, expecting (");
  EXPECT_T("a IN (b, a)");
  EXPECT_F("a in (b, c)");
  EXPECT_T("'a' In ('b', c, $.a)");
  EXPECT_F("a not in (a)");
  EXPECT_F("a NOT IN (b, a)");
  EXPECT_T("a not in (b, c)");
  EXPECT_F("a Not In ('b', 'c', $.a)");

  EXPECT_T("10 in (1, 3+4, 2*5)");
  EXPECT_F("10 in (10-1, 3+4, 2*6)");
  EXPECT_T("10 not in (10-1, sqrt(3+4), 2*6)");
  EXPECT_T(
      "MYSQL in ($.mysql, postgres, mongo) AND $.postgres not in (\"Linux\", "
      "'Windows XP', MacOS)");
}

TEST_F(Routing_guidelines_parser_test, ip_functions) {
  // Execution without cache will fail
  EXPECT_EE("resolve_v4  (localhost)",
            "No cache entry to resolve host: localhost");
  EXPECT_EE("resolve_v4('oracle.com')",
            "No cache entry to resolve host: oracle");
  EXPECT_EE("resolve_v6('oracle.com')",
            "No cache entry to resolve host: oracle");

  // invalid hostnames
  EXPECT_PE("resolve_v4('oracle_com')",
            "RESOLVE_V4 function, invalid hostname: 'oracle_com'");
  EXPECT_PE("resolve_v6('oracle_com')",
            "RESOLVE_V6 function, invalid hostname: 'oracle_com'");

  // only string literals are allowed by resolve function
  ctx.set("host", "oracle.com");
  EXPECT_PE(
      "resolve_v4($.host)",
      "RESOLVE_V4 function only accepts string literals as its parameter");
  EXPECT_PE(
      "resolve_v6($.host)",
      "RESOLVE_V6 function only accepts string literals as its parameter");

  cache.emplace("abra",
                net::ip::make_address_v6("2001:db8::1428:57ab").value());
  cache.emplace("localhost", net::ip::make_address("7.7.7.7").value());

  // Execution suceeds for cached entries
  EXPECT_STR("2001:db8::1428:57ab", "resolve_v6(abra)");
  EXPECT_STR("7.7.7.7", "resolve_v4 ('localhost')");

  // Still fails for missing values
  EXPECT_EE("resolve_v4('oracle.com')",
            "No cache entry to resolve host: oracle");
  EXPECT_EE("resolve_v6('oracle.com')",
            "No cache entry to resolve host: oracle");

  EXPECT_STR("128.128.0.0", "network ('128.128.128.128', 16)");
  EXPECT_STR("221.221.221.0", "network('221.221.221.128', 24)");
  EXPECT_STR("221.0.0.0", "network('221.221.221.128', 8)");

  EXPECT_T("is_ipv4('0.0.0.0')");
  EXPECT_T("is_ipv4('127.0.0.1')");
  EXPECT_T("is_ipv4('255.255.255.255')");
  EXPECT_T("is_ipv4('000.000.000.000')");
  EXPECT_T("is_ipv4('0x7F.0.0.1')");

  EXPECT_F("is_ipv4('')");
  EXPECT_F("is_ipv4('localhost')");
  EXPECT_F("is_ipv4('google.pl')");
  EXPECT_F("is_ipv4('::8.8.8.8')");
  EXPECT_F("is_ipv4('255.255.255.256')");
  EXPECT_F("is_ipv4('2010:836B:4179::836B:4179')");
  EXPECT_F("is_ipv4('FEDC:BA98:7654:3210:FEDC:BA98:7654:3210')");

  EXPECT_T("is_ipv6('FEDC:BA98:7654:3210:FEDC:BA98:7654:3210')");
  EXPECT_T("is_ipv6('1080:0:0:0:8:800:200C:4171')");
  EXPECT_T("is_ipv6('3ffe:2a00:100:7031::1')");
  EXPECT_T("is_ipv6('1080::8:800:200C:417A')");
  EXPECT_T("is_ipv6('::192.9.5.5')");
  EXPECT_T("is_ipv6('::1')");
  EXPECT_T("is_ipv6('2010:836B:4179::836B:4179')");

  // IPv6 scoped addressing zone identifiers
  EXPECT_T("is_ipv6('fe80::850a:5a7c:6ab7:aec4%1')");
  EXPECT_T("is_ipv6('fe80::850a:5a7c:6ab7:aec4%eth0')");
  EXPECT_T("is_ipv6('fe80::850a:5a7c:6ab7:aec4%enp0s3')");

  EXPECT_F("is_ipv6('')");
  EXPECT_F("is_ipv6('localhost')");
  EXPECT_F("is_ipv6('google.pl')");
  EXPECT_F("is_ipv6('unknown_host')");
  EXPECT_F("is_ipv6('127.0.0.1')");
  EXPECT_F("is_ipv6('FEDC:BA98:7654:3210:FEDC:BA98:7654:3210:')");
  EXPECT_F("is_ipv6('FEDC:BA98:7654:3210:GEDC:BA98:7654:3210')");
}

TEST_F(Routing_guidelines_parser_test, type_errors) {
  EXPECT_PE("sqrt('a')", "SQRT function, expected NUMBER but got STRING");
  EXPECT_PE("sqrt(PRIMARY)", "got ROLE");

  EXPECT_PE(
      "regexp_like('a', 2)",
      "REGEXP_LIKE function, 2nd argument, expected STRING but got NUMBER");
  EXPECT_PE(
      "regexp_like(TRUE, 3)",
      "REGEXP_LIKE function, 1st argument, expected STRING but got BOOLEAN");

  EXPECT_PE("resolve_v4(1.1)",
            "RESOLVE_V4 function, expected STRING but got NUMBER");
  EXPECT_PE("resolve_v6(1.1)",
            "RESOLVE_V6 function, expected STRING but got NUMBER");

  EXPECT_PE("network('a', TRUE)",
            "NETWORK function, 2nd argument, expected NUMBER but got BOOLEAN");
  EXPECT_PE("network(1, 3)",
            "NETWORK function, 1st argument, expected STRING but got NUMBER");
  EXPECT_THROW_LIKE(parse_eval("network('foo', 16)"), std::runtime_error,
                    "invalid IPv4");

  EXPECT_PE(
      "SUBSTRING_INDEX('www.mysql.com', '.', '-3')",
      "SUBSTRING_INDEX function, 3rd argument, expected NUMBER but got STRING");
  EXPECT_PE(
      "substring_index('www.mysql.com', 2, -3)",
      "SUBSTRING_INDEX function, 2nd argument, expected STRING but got NUMBER");

  EXPECT_PE(
      "startswith('www.mysql.com', 2)",
      "STARTSWITH function, 2nd argument, expected STRING but got NUMBER");
  EXPECT_PE("endswith(2, 'dwa')",
            "ENDSWITH function, 1st argument, expected STRING but got NUMBER");

  EXPECT_PE("2+'a'",
            "+ operator, right operand, expected NUMBER but got STRING");
  EXPECT_PE("PRIMARY * 3",
            "* operator, left operand, expected NUMBER but got ROLE");
  EXPECT_PE("abra / 3",
            "/ operator, left operand, expected NUMBER but got STRING");
  EXPECT_PE("3 - true",
            "- operator, right operand, expected NUMBER but got BOOLEAN");
  EXPECT_PE("12 % abra",
            "% operator, right operand, expected NUMBER but got STRING");
  EXPECT_PE("-abra", "- operator, expected NUMBER but got STRING");

  EXPECT_PE(
      "2='a'",
      "= operator, the type of left operand does not match right, expected "
      "NUMBER but got STRING");
  EXPECT_PE(
      "PRIMARY <> 3",
      "<> operator, the type of left operand does not match right, expected "
      "ROLE but got NUMBER");
  EXPECT_PE(
      "abra >= 3",
      ">= operator, the type of left operand does not match right, expected "
      "STRING but got NUMBER");
  EXPECT_PE(
      "3 > true",
      "> operator, the type of left operand does not match right, expected "
      "NUMBER but got BOOLEAN");
  EXPECT_PE(
      "abra <= 3",
      "<= operator, the type of left operand does not match right, expected "
      "STRING but got NUMBER");
  EXPECT_PE(
      "3 < true",
      "< operator, the type of left operand does not match right, expected "
      "NUMBER but got BOOLEAN");
  EXPECT_PE(
      "false < true",
      "type error, BOOLEAN type arguments cannot be compared with < operator");
  EXPECT_PE(
      "PRIMARY >= SECONDARY",
      "type error, ROLE type arguments cannot be compared with >= operator");

  ctx.set("a", 10);
  EXPECT_PE(
      "3 in (2-$.a, true)",
      "in operator, type of element at offset 1 does not match the type of "
      "searched element, expected NUMBER but got BOOLEAN");
  EXPECT_PE(
      "abra in (PRIMARY)",
      "in operator, type of element at offset 0 does not match the type of "
      "searched element, expected STRING but got ROLE");
  EXPECT_PE(
      "abra not in ('a', b, sqrt($.a))",
      "in operator, type of element at offset 2 does not match the type of "
      "searched element, expected STRING but got NUMBER");

  EXPECT_PE("1 like ala",
            "LIKE operator, left operand, expected STRING but got NUMBER");
  EXPECT_PE("ala like 1",
            "LIKE operator, right operand, expected STRING but got NUMBER");
}

TEST_F(Routing_guidelines_parser_test, syntax_errors) {
  EXPECT_PE("SQR()",
            "syntax error, unexpected (, expecting end of expression or error");

  EXPECT_PE("SQRT()",
            "syntax error, function SQRT expected 1 argument but got none");
  EXPECT_PE("network('127.0.0.1')",
            "syntax error, function NETWORK expected 2 arguments but got 1");
  EXPECT_PE("RESOLVE_V4('127.0.0.1', 12)",
            "syntax error, function RESOLVE_V4 expected 1 argument but got 2");
  EXPECT_PE("RESOLVE_V6('127.0.0.1', 12)",
            "syntax error, function RESOLVE_V6 expected 1 argument but got 2");
  EXPECT_PE(
      "regexp_like('127.0.0.1', 12, 13)",
      "syntax error, function REGEXP_LIKE expected 2 arguments but got 3");

  EXPECT_PE("2+3=", "syntax error, unexpected end of expression (character 4)");
  EXPECT_PE("sqrt(2",
            "syntax error, unexpected end of expression, expecting ) "
            "or \",\" (character 6)");
  EXPECT_PE(
      "3 in resolve_v4(localhost)",
      "syntax error, unexpected function name, expecting ( in 'resolve_v4'");
  EXPECT_PE(
      "3 in resolve_v6(localhost)",
      "syntax error, unexpected function name, expecting ( in 'resolve_v6'");
  EXPECT_PE("a==2", "syntax error, unexpected = (character 3)");
  EXPECT_PE("a!=2", "syntax error, unexpected character: '!' (character 2)");
  EXPECT_PE("3 < 4 > 5", "syntax error, unexpected > (character 7)");

  EXPECT_PE("endswith(2, 'dwa)", "syntax error, unclosed ' (character 13)");
}

TEST_F(Routing_guidelines_parser_test, rpn_expressions_comparison) {
  ctx.set("a", 10);
  ctx.set("b", 10);
  ctx.set("dwa", 2);
  ctx.set("Johnny.sh", "Johnny S");
  ctx.set("mysql", "mysql");
  ctx.set("postgres", "postgres");

  auto exp1 = parse("$.a + $.dwa + 3-10*0.1");
  EXPECT_TRUE(exp1 == parse("$.a + $.dwa + 3-10*0.1"));
  EXPECT_FALSE(exp1 == parse("$.b + $.dwa + 3-10*0.1"));
  EXPECT_FALSE(exp1 == parse("$.a + 2 + 3-10*0.1"));
  EXPECT_FALSE(exp1 == parse("$.a + $.dwa + 4-10*0.1"));
  EXPECT_FALSE(exp1 == parse("$.a + $.dwa + 3-10*0.2"));
  EXPECT_FALSE(exp1 == parse("$.a + $.dwa - 3-10*0.1"));
  EXPECT_FALSE(exp1 == parse("$.a + $.dwa + 10*0.1"));

  auto exp2 = parse(
      "'Johnny' IN (\"This is a test!\", 'Judy', $.Johnny.sh) OR "
      "regexp_like(subject, '(sub)(.*)')");
  EXPECT_TRUE(exp2 ==
              parse("'Johnny' IN (\"This is a test!\", 'Judy', $.Johnny.sh) OR "
                    "regexp_like(subject, '(sub)(.*)')"));
  // True cause regexp_like gets preevaluated to true during evaluations
  EXPECT_TRUE(exp2 ==
              parse("'Johnny' IN (\"This is a test!\", 'Judy', $.Johnny.sh) OR "
                    "regexp_like(subject, '(su)(.*)')"));
  EXPECT_FALSE(
      exp2 == parse("'Johnny' IN (\"This is a test!\", 'Judy', $.Johnny.sh) OR "
                    "regexp_like(subject, '(ru)(.*)')"));
  EXPECT_TRUE(exp2 ==
              parse("'Johnny' IN (\"This is a test!\", 'Judy', $.Johnny.sh) OR "
                    "regexp_like(subject, '(sub)(.*)')"));
  EXPECT_FALSE(
      exp2 ==
      parse("'Johnny' not IN (\"This is a test!\", 'Judy', $.Johnny.sh) OR "
            "regexp_like(subject, '(sub)(.*)')"));

  auto exp3 = parse(
      "'Johnny' IN (\"This is a test!\", 'Judy', $.Johnny.sh) OR "
      "regexp_like($.mysql, '(sub)(.*)')");

  // False cause regexp is not preevaluated and arguments differ
  EXPECT_FALSE(
      exp3 == parse("'Johnny' IN (\"This is a test!\", 'Judy', $.Johnny.sh) OR "
                    "regexp_like($.mysql, '(su)(.*)')"));
  EXPECT_FALSE(
      exp3 == parse("'Johnny' IN (\"This is a test!\", 'Judy', $.Johnny.sh) OR "
                    "regexp_like($.postgres, '(sub)(.*)')"));

  // Function changed
  EXPECT_FALSE(
      exp3 == parse("'Johnny' IN (\"This is a test!\", 'Judy', $.Johnny.sh) OR "
                    "endswith($.mysql, '(sub)(.*)')"));
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

  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
