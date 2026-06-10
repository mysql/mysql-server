/* Copyright (c) 2014, 2026, Oracle and/or its affiliates.

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

#include "my_config.h"

#include <gtest/gtest.h>

#include "sql/sql_class.h"
#include "string_with_len.h"
#include "unittest/gunit/test_utils.h"

namespace security_context_unittest {

/*
  Testing accessor functions of string type data members of class
  Security_context.
*/
TEST(Security_context, string_data_member) {
  Security_context sctx;

  // Case 1: Initialize Security context and check the values set.
  EXPECT_EQ(sctx.user().length, (size_t)0);
  EXPECT_EQ(sctx.host().length, (size_t)0);
  EXPECT_EQ(sctx.ip().length, (size_t)0);
  EXPECT_EQ(sctx.external_user().length, (size_t)0);
  EXPECT_EQ(strcmp(sctx.host_or_ip().str, "connecting host"), 0);
  EXPECT_EQ(sctx.priv_user().length, (size_t)0);
  EXPECT_EQ(sctx.proxy_user().length, (size_t)0);
  EXPECT_EQ(sctx.priv_host().length, (size_t)0);

  // Case 2: Set the empty string to Securtiy context members and check values.
  sctx.set_user_ptr("", 0);
  sctx.set_host_ptr("", 0);
  sctx.set_ip_ptr("", 0);
  sctx.set_host_or_ip_ptr();
  sctx.set_external_user_ptr("", 0);
  sctx.assign_priv_user("", 0);
  sctx.assign_proxy_user("", 0);
  sctx.assign_priv_host("", 0);

  EXPECT_EQ(sctx.user().length, (size_t)0);
  EXPECT_EQ(sctx.host().length, (size_t)0);
  EXPECT_EQ(sctx.ip().length, (size_t)0);
  EXPECT_EQ(sctx.external_user().length, (size_t)0);
  EXPECT_EQ(sctx.host_or_ip().length, (size_t)0);
  EXPECT_EQ(sctx.priv_user().length, (size_t)0);
  EXPECT_EQ(sctx.proxy_user().length, (size_t)0);
  EXPECT_EQ(sctx.priv_host().length, (size_t)0);

  // using method assign_xxxx();
  sctx.assign_user("", 0);
  sctx.assign_host("", 0);
  sctx.assign_ip("", 0);
  sctx.assign_external_user("", 0);

  EXPECT_EQ(sctx.user().length, (size_t)0);
  EXPECT_EQ(sctx.host().length, (size_t)0);
  EXPECT_EQ(sctx.ip().length, (size_t)0);
  EXPECT_EQ(sctx.external_user().length, (size_t)0);
  EXPECT_EQ(sctx.host_or_ip().length, (size_t)0);
  EXPECT_EQ(sctx.priv_user().length, (size_t)0);
  EXPECT_EQ(sctx.proxy_user().length, (size_t)0);
  EXPECT_EQ(sctx.priv_host().length, (size_t)0);

  // using  method assign_host() but passing the nullptr to it
  sctx.assign_host(nullptr, 0);
  EXPECT_EQ(sctx.host().length, (size_t)0);

  // Case 3: Set non-empty string to Securtiy context members and check values.
  sctx.set_user_ptr(STRING_WITH_LEN("user_test"));
  sctx.set_host_ptr(STRING_WITH_LEN("localhost"));
  sctx.set_ip_ptr(STRING_WITH_LEN("127.0.0.1"));
  sctx.set_host_or_ip_ptr();
  sctx.set_external_user_ptr(STRING_WITH_LEN("ext_user_test"));
  sctx.assign_priv_user(STRING_WITH_LEN("priv_user"));
  sctx.assign_proxy_user(STRING_WITH_LEN("proxy_user"));
  sctx.assign_priv_host(STRING_WITH_LEN("localhost"));

  EXPECT_EQ(0, strcmp(sctx.user().str, "user_test"));
  EXPECT_EQ(0, strcmp(sctx.host().str, "localhost"));
  EXPECT_EQ(0, strcmp(sctx.ip().str, "127.0.0.1"));
  EXPECT_EQ(0, strcmp(sctx.external_user().str, "ext_user_test"));
  EXPECT_EQ(0, strcmp(sctx.host_or_ip().str, "localhost"));
  EXPECT_EQ(0, strcmp(sctx.priv_user().str, "priv_user"));
  EXPECT_EQ(0, strcmp(sctx.proxy_user().str, "proxy_user"));
  EXPECT_EQ(0, strcmp(sctx.priv_host().str, "localhost"));

  sctx.set_host_or_ip_ptr(sctx.ip().str, sctx.ip().length);
  EXPECT_EQ(0, strcmp(sctx.host_or_ip().str, "127.0.0.1"));

  // Case 4: Change members with non-empty string and check values.
  sctx.set_user_ptr(STRING_WITH_LEN("user_test_1"));
  sctx.set_host_ptr(STRING_WITH_LEN("localhost_1"));
  sctx.set_ip_ptr(STRING_WITH_LEN("127.0.0.2"));
  sctx.set_host_or_ip_ptr();
  sctx.set_external_user_ptr(STRING_WITH_LEN("ext_user_test_1"));
  sctx.assign_priv_user(STRING_WITH_LEN("priv_user_1"));
  sctx.assign_proxy_user(STRING_WITH_LEN("proxy_user_1"));
  sctx.assign_priv_host(STRING_WITH_LEN("localhost_1"));

  EXPECT_EQ(0, strcmp(sctx.user().str, "user_test_1"));
  EXPECT_EQ(0, strcmp(sctx.host().str, "localhost_1"));
  EXPECT_EQ(0, strcmp(sctx.ip().str, "127.0.0.2"));
  EXPECT_EQ(0, strcmp(sctx.external_user().str, "ext_user_test_1"));
  EXPECT_EQ(0, strcmp(sctx.host_or_ip().str, "localhost_1"));
  EXPECT_EQ(0, strcmp(sctx.priv_user().str, "priv_user_1"));
  EXPECT_EQ(0, strcmp(sctx.proxy_user().str, "proxy_user_1"));
  EXPECT_EQ(0, strcmp(sctx.priv_host().str, "localhost_1"));

  // Case 5: Change members with non-empty string members with copy option.
  sctx.assign_user(STRING_WITH_LEN("user_test"));
  sctx.assign_host(STRING_WITH_LEN("localhost"));
  sctx.assign_ip(STRING_WITH_LEN("127.0.0.1"));
  sctx.set_host_or_ip_ptr();
  sctx.assign_external_user(STRING_WITH_LEN("ext_user_test"));
  sctx.assign_priv_user(STRING_WITH_LEN("priv_user"));
  sctx.assign_proxy_user(STRING_WITH_LEN("proxy_user"));
  sctx.assign_priv_host(STRING_WITH_LEN("localhost"));

  EXPECT_EQ(0, strcmp(sctx.user().str, "user_test"));
  EXPECT_EQ(0, strcmp(sctx.host().str, "localhost"));
  EXPECT_EQ(0, strcmp(sctx.ip().str, "127.0.0.1"));
  EXPECT_EQ(0, strcmp(sctx.external_user().str, "ext_user_test"));
  EXPECT_EQ(0, strcmp(sctx.host_or_ip().str, "localhost"));
  EXPECT_EQ(0, strcmp(sctx.priv_user().str, "priv_user"));
  EXPECT_EQ(0, strcmp(sctx.proxy_user().str, "proxy_user"));
  EXPECT_EQ(0, strcmp(sctx.priv_host().str, "localhost"));
}

TEST(Security_context, user_membership_checks) {
  Security_context sctx;
  sctx.assign_priv_user(STRING_WITH_LEN("user1"));
  sctx.assign_priv_host(STRING_WITH_LEN("example.com"));

  constexpr std::string_view auth_id_list_1 =
      "`user1`@`localhost`, `user1`@`%`, `user2`@`example.com`, ``@``, "
      "`user1`@`example.com`";
  constexpr std::string_view auth_id_list_2 =
      "`user1`@`localhost`, `user1`@`%`, `user2`@`example.com`, ``@``, "
      "`User1`@`example.com`";
  constexpr std::string_view auth_id_list_3 =
      "user1@localhost, user1, user2@example.com, ``@``, user1@example.com";
  constexpr std::string_view auth_id_list_4 =
      "user1@localhost, user1, user2@example.com, ``@``, User1@example.com";
  constexpr std::string_view auth_id_list_5 =
      "user1@localhost, user2, user2@example.com, ``@``, User1@example.com";

  EXPECT_TRUE(sctx.is_current_user_part_of(auth_id_list_1));
  EXPECT_FALSE(sctx.is_current_user_part_of(auth_id_list_2));
  EXPECT_TRUE(sctx.is_current_user_part_of(auth_id_list_3));
  EXPECT_FALSE(sctx.is_current_user_part_of(auth_id_list_4));
  EXPECT_FALSE(sctx.is_current_user_part_of(auth_id_list_5));

  sctx.assign_priv_host(STRING_WITH_LEN("%"));

  EXPECT_TRUE(sctx.is_current_user_part_of(auth_id_list_1));
  EXPECT_TRUE(sctx.is_current_user_part_of(auth_id_list_2));
  EXPECT_TRUE(sctx.is_current_user_part_of(auth_id_list_3));
  EXPECT_TRUE(sctx.is_current_user_part_of(auth_id_list_4));
  EXPECT_FALSE(sctx.is_current_user_part_of(auth_id_list_5));
}

TEST(Security_context, role_membership_checks) {
  Security_context sctx;
  sctx.assign_priv_user(STRING_WITH_LEN("user1"));
  sctx.assign_priv_host(STRING_WITH_LEN("example.com"));

  LEX_CSTRING role1{STRING_WITH_LEN("role1")};
  LEX_CSTRING role1_host{STRING_WITH_LEN("%")};

  LEX_CSTRING role2{STRING_WITH_LEN("role2")};
  LEX_CSTRING role2_host{STRING_WITH_LEN("%")};

  LEX_CSTRING role3{STRING_WITH_LEN("role3")};
  LEX_CSTRING role3_host{STRING_WITH_LEN("%")};

  LEX_CSTRING role4{STRING_WITH_LEN("role4")};
  LEX_CSTRING role4_host{STRING_WITH_LEN("%")};

  LEX_CSTRING role5{STRING_WITH_LEN("role5")};
  LEX_CSTRING role5_host{STRING_WITH_LEN("%")};

  EXPECT_EQ(0, sctx.activate_role(role1, role1_host, false));
  EXPECT_EQ(0, sctx.activate_role(role2, role2_host, false));
  EXPECT_EQ(0, sctx.activate_role(role3, role3_host, false));
  EXPECT_EQ(0, sctx.activate_role(role4, role4_host, false));
  EXPECT_EQ(0, sctx.activate_role(role5, role5_host, false));

  constexpr std::string_view role_list_1 =
      "`role_6`@`%`, `role7`@``, `role1`@`example.com`, `role1`@`%`, "
      "`role3`@`%`";
  constexpr std::string_view role_list_2 =
      "`role_6`@`%`, `role7`@``, `role1`@`example.com`, `role3`";
  constexpr std::string_view role_list_3 =
      "`role_6`@`%`, `role7`@``, `role1`@`example.com`, `role8`@`%`";
  constexpr std::string_view role_list_4 = "role6, role3, role9";

  std::string first_role{};

  EXPECT_TRUE(sctx.is_current_role_part_of(role_list_1, &first_role));
  EXPECT_EQ(first_role, "`role1`@`%`");

  first_role.clear();
  EXPECT_TRUE(sctx.is_current_role_part_of(role_list_2, &first_role));
  EXPECT_EQ(first_role, "`role3`@`%`");

  first_role.clear();
  EXPECT_FALSE(sctx.is_current_role_part_of(role_list_3, &first_role));
  EXPECT_EQ(first_role, "");

  first_role.clear();
  EXPECT_TRUE(sctx.is_current_role_part_of(role_list_4, &first_role));
  EXPECT_EQ(first_role, "`role3`@`%`");
}

}  // namespace security_context_unittest
