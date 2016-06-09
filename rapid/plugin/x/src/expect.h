/*
 * Copyright (c) 2015, 2016, Oracle and/or its affiliates. All rights reserved.
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

#ifndef _XPL_EXPECT_H_
#define _XPL_EXPECT_H_

#include "ngs/protocol_encoder.h"

#include <list>

#include "ngs_common/protocol_protobuf.h"

namespace xpl
{
  class Expect_condition
  {
  public:
    virtual ~Expect_condition() {}
    virtual Expect_condition *copy() = 0;
    virtual ngs::Error_code check() = 0;

    uint32_t key() const { return m_key; }
    void set_key(uint32_t k) { m_key = k; }
  private:
    uint32_t m_key;
  };

  class Expectation
  {
  public:
    Expectation() : m_fail_on_error(false), m_gtid_wait_less_than(0) {}
    Expectation(const Expectation &other);

    ~Expectation();

    Expectation &operator =(const Expectation &other);

    // whether an error occurred previously in a no_error block
    void set_failed(const std::string &reason) { m_failed = reason; }
    std::string failed_condition() const { return m_failed; }
    bool failed() const { return !m_failed.empty(); }
    bool fail_on_error() const { return m_fail_on_error; }

    ngs::Error_code check();

    void unset(uint32_t key);
    ngs::Error_code set(uint32_t key, const std::string &value);

    void swap(Expectation &one, Expectation &other);

    void add_condition(Expect_condition *cond);
  private:
    std::list<Expect_condition*> m_conditions;
    std::string m_failed;
    bool m_fail_on_error;

    int m_gtid_wait_less_than;
  };

  class Expectation_stack
  {
  public:
    Expectation_stack();

    ngs::Error_code open(const Mysqlx::Expect::Open &open);
    ngs::Error_code close();

    // called before executing client statements. should signal error if one is returned
    ngs::Error_code pre_client_stmt(int8_t msgid);

    // called when an error occurs executing client statements
    void post_client_stmt(int8_t msgid, const ngs::Error_code &error);

  private:
    std::vector<Expectation> m_expect_stack;
  };
}


#endif
