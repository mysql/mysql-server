/*
 * Copyright (c) 2015, 2016 Oracle and/or its affiliates. All rights reserved.
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

#include "expect.h"
#include "xpl_error.h"
#include "ngs_common/protocol_protobuf.h"
//#include "expect_gtid.h"

using namespace xpl;

// :docnote:
// NO_ERROR means "enable exceptions", meaning any error that happens inside a block
// will cause all subsequent statements to fail until the matching close is found.
//
// Nesting behaviour:
//
// Case 1: No_error
//
// open(NO_ERROR) - ok
//   stmt1 - ok
//   stmt2 - error
//   stmt3 - fail
// close() - fail
//
// Case 2: Plain
//
// open() - ok
//   stmt1 - ok
//   stmt2 - error
//   stmt3 - ok
// close() - ok
//
// Case 3: No_error nested within no_error
//
// 3.1: error in outer block fails the whole thing
// open(NO_ERROR) - ok
//   stmt1 - error
//   open(NO_ERROR) - fail
//     stmt2 - fail
//   close() - fail
//   stmt3 - fail
// close() - fail
//
// 3.2: error propagates up and fails the outer block
// open(NO_ERROR) - ok
//   stmt1 - ok
//   open(NO_ERROR) - ok
//     stmt2 - error
//   close() - fail
//   stmt3 - fail
// close() - fail
//
// Case 4: Plain nested within no_error
//
// 4.1: same as 3.1
// open(NO_ERROR) - ok
//   stmt1 - error
//   open() - fail
//     stmt2 - fail
//   close() - fail
//   stmt3 - fail
// close() - fail
//
// 4.2: plain block effectively "catches" the error and prevents it from failing the outer block
// open(NO_ERROR) - ok
//   stmt1 - ok
//   open() - ok
//     stmt2 - error
//   close() - ok
//   stmt3 - ok
// close() - ok
//
// Case 5: No_error nested within Plain
//
// 5.1: trivial
// open() - ok
//   stmt1 - error
//   open(NO_ERROR) - ok
//     stmt2 - ok
//   close() - ok
//   stmt3 - ok
// close() - ok
//
// 5.2: error propagates up, but is ignored by the outer block
// open() - ok
//   stmt1 - ok
//   open(NO_ERROR) - ok
//     stmt2 - error
//   close() - fail
//   stmt3 - ok
// close() - ok
//
// Case 6: Plain nested within plain: trivial, behaves like a flat plain block
//


static const unsigned EXPECT_NO_ERROR = 1;
//static const int EXPECT_SCHEMA_VERSION = 2; // not supported yet
//static const int EXPECT_GTID_EXECUTED_CONTAINS = 3;
//static const int EXPECT_GTID_WAIT_LESS_THAN = 4;



// if (pre_client_stmt(msgid))
// {
//   error = ExecuteCommand()
//   post_client_stmt(msgid, error)
// }


Expectation::Expectation(const Expectation &other)
: m_failed(other.m_failed), m_fail_on_error(other.m_fail_on_error), m_gtid_wait_less_than(0) // this is instance specific data, don't copy it
{
  for (std::list<Expect_condition*>::const_iterator cond = other.m_conditions.begin();
       cond != other.m_conditions.end(); ++cond)
  {
    m_conditions.push_back((*cond)->copy());
  }
}


void Expectation::swap(Expectation &one, Expectation &other)
{
  using std::swap;
  swap(one.m_failed, other.m_failed);
  swap(one.m_fail_on_error, other.m_fail_on_error);
  swap(one.m_conditions, other.m_conditions);
}


Expectation &Expectation::operator =(const Expectation &other)
{
  Expectation tmp(other);
  swap(*this, tmp);
  return *this;
}


Expectation::~Expectation()
{
  for (std::list<Expect_condition*>::iterator cond = m_conditions.begin();
       cond != m_conditions.end(); ++cond)
    delete *cond;
}


ngs::Error_code Expectation::check()
{
  for (std::list<Expect_condition*>::const_iterator cond = m_conditions.begin();
       cond != m_conditions.end(); ++cond)
  {
    ngs::Error_code error((*cond)->check());
    if (error)
      return error;
  }
  return ngs::Error_code();
}


void Expectation::unset(uint32_t key)
{
  if (key == EXPECT_NO_ERROR)
  {
    m_fail_on_error = false;
    return;
  }

  for (std::list<Expect_condition*>::iterator cond = m_conditions.begin();
       cond != m_conditions.end(); ++cond)
  {
    if ((*cond)->key() == key)
    {
      delete *cond;
      m_conditions.erase(cond);
      break;
    }
  }
}


void Expectation::add_condition(Expect_condition *cond)
{
  m_conditions.push_back(cond);
}


ngs::Error_code Expectation::set(uint32_t key, const std::string &value)
{
  switch (key)
  {
    case EXPECT_NO_ERROR:
      if (value == "1" || value.empty())
        m_fail_on_error = true;
      else if (value == "0")
        m_fail_on_error = false;
      else
        return ngs::Error_code(ER_X_EXPECT_BAD_CONDITION_VALUE, "Invalid value '"+value+"' for expectation no_error");
      break;
/*
    case EXPECT_GTID_EXECUTED_CONTAINS:
    {
      Expect_gtid *gtid;
      add_condition(gtid = new Expect_gtid(value));
      gtid->set_key(key);
      // timeout was set first
      if (m_gtid_wait_less_than > 0)
        gtid->set_timeout(m_gtid_wait_less_than);
      break;
    }
    case EXPECT_GTID_WAIT_LESS_THAN:
      m_gtid_wait_less_than = ngs::stoi(value);
      for (std::list<Expect_condition*>::iterator cond = m_conditions.begin();
           cond != m_conditions.end(); ++cond)
      {
        if ((*cond)->key() == EXPECT_GTID_EXECUTED_CONTAINS)
        {
          static_cast<Expect_gtid*>(*cond)->set_timeout(m_gtid_wait_less_than);
          break;
        }
      }
      break;
*/
    default:
      return ngs::Error_code(ER_X_EXPECT_BAD_CONDITION, "Unknown condition key");
  }
  return ngs::Error_code();
}




Expectation_stack::Expectation_stack()
{
  m_expect_stack.reserve(4);
}


ngs::Error_code Expectation_stack::open(const Mysqlx::Expect::Open &open)
{
  ngs::Error_code error;
  Expectation expect;

  // if we're in a failed expect block, push an empty failed expectation to the stack
  // so that it can be popped when the matching close is seen.
  // No other evaluations are done in a failed state.
  if (!m_expect_stack.empty())
  {
    if (m_expect_stack.back().failed())
    {
      expect.set_failed(m_expect_stack.back().failed_condition());
      m_expect_stack.push_back(expect);
      return ngs::Error_code(ER_X_EXPECT_FAILED, "Expectation failed: "+expect.failed_condition());
    }

    if (open.op() == Mysqlx::Expect::Open::EXPECT_CTX_COPY_PREV)
      expect = m_expect_stack.back();
  }

  for (int i = 0; i < open.cond_size(); i++)
  {
    const Mysqlx::Expect::Open::Condition &cond(open.cond(i));
    switch (cond.op())
    {
      case Mysqlx::Expect::Open::Condition::EXPECT_OP_SET:
        if (!cond.has_condition_value())
          error = expect.set(cond.condition_key(), "");
        else
          error = expect.set(cond.condition_key(), cond.condition_value());
        break;
      case Mysqlx::Expect::Open::Condition::EXPECT_OP_UNSET:
        expect.unset(cond.condition_key());
        break;
    }
    if (error)
      return error;
  }
  // we need to add the expectation block even if an error occurred,
  // otherwise we'll get mismatched open/close blocks
  // on_error should get called afterwards with this error, which should fail the rest of the block
  m_expect_stack.push_back(expect);

  // now check for the expected conditions
  // this may block if a blocking condition is found
  if (!error)
    error = expect.check();

  return error;
}


ngs::Error_code Expectation_stack::close()
{
  if (m_expect_stack.empty())
    return ngs::Error_code(ER_X_EXPECT_NOT_OPEN, "Expect block currently not open");

  if (m_expect_stack.back().failed())
  {
    std::string cond = m_expect_stack.back().failed_condition();
    m_expect_stack.pop_back();
    return ngs::Error_code(ER_X_EXPECT_FAILED, "Expectation failed: " + cond);
  }

  m_expect_stack.pop_back();

  return ngs::Error_code();
}


// called before executing client statements
ngs::Error_code Expectation_stack::pre_client_stmt(int8_t msgid)
{
  if (!m_expect_stack.empty())
  {
    if (m_expect_stack.back().failed())
    {
      // special handling for nested expect blocks
      // if a block open or close arrives in a failed state, we let it through
      // so that they can be pushed/popped on the stack and properly accounted for
      if (msgid != Mysqlx::ClientMessages::EXPECT_OPEN && msgid != Mysqlx::ClientMessages::EXPECT_CLOSE)
        return ngs::Error_code(ER_X_EXPECT_FAILED, "Expectation failed: " + m_expect_stack.back().failed_condition());
    }
  }
  return ngs::Error_code();
}


// called after executing client statements
void Expectation_stack::post_client_stmt(int8_t msgid, const ngs::Error_code &error)
{
  if (error && !m_expect_stack.empty() && m_expect_stack.back().fail_on_error())
    m_expect_stack.back().set_failed("no_error");
}
