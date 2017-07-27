/*
 * Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; version 2 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301  USA
 */

#ifndef X_SRC_EXPECT_EXPECT_CONDITION_FIELD_H
#define X_SRC_EXPECT_EXPECT_CONDITION_FIELD_H

#include <string>
#include "expect/expect_condition.h"


namespace xpl {

class Expect_condition_field: public Expect_condition {
 public:
  explicit Expect_condition_field(const std::string &expected_field);
  Expect_condition_field(const Expect_condition_field &other);

  Expect_condition_ptr clone() override;
  ngs::Error_code check_if_error() override;
};

}  // namespace xpl

#endif  // X_SRC_EXPECT_EXPECT_CONDITION_FIELD_H
