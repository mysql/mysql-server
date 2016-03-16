/* Copyright (c) 2015, 2016 Oracle and/or its affiliates. All rights reserved.

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; version 2 of the License.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */


#include <gtest/gtest.h>
#include "expect.h"
#include "xpl_error.h"
#include "ngs_common/protocol_protobuf.h"


static const int EXPECT_NO_ERROR = 1;

namespace xpl
{
  namespace test
  {
    static ngs::Error_code success;

    static ngs::Error_code simulate_instruction(Expectation_stack &xs, int8_t mid, ngs::Error_code fail)
    {
      ngs::Error_code err = xs.pre_client_stmt(mid);
      if (!err)
      {
        // execution would come here
        err = fail;
        xs.post_client_stmt(mid, fail);
      }
      return err;
    }

    static ngs::Error_code simulate_close(Expectation_stack &xs)
    {
      ngs::Error_code err = xs.pre_client_stmt(Mysqlx::ClientMessages::EXPECT_CLOSE);
      if (!err)
      {
        err = xs.close();
        xs.post_client_stmt(Mysqlx::ClientMessages::EXPECT_CLOSE, err);
      }
      return err;
    }

    static ngs::Error_code simulate_open(Expectation_stack &xs, const Mysqlx::Expect::Open &open)
    {
      ngs::Error_code err = xs.pre_client_stmt(Mysqlx::ClientMessages::EXPECT_OPEN);
      if (!err)
      {
        err = xs.open(open);
        xs.post_client_stmt(Mysqlx::ClientMessages::EXPECT_OPEN, err);
      }
      return err;
    }

    static Mysqlx::Expect::Open Inherit()
    {
      Mysqlx::Expect::Open open;
      open.set_op(Mysqlx::Expect::Open::EXPECT_CTX_COPY_PREV);
      return open;
    }

    static Mysqlx::Expect::Open Noerror()
    {
      Mysqlx::Expect::Open open;

      open.set_op(Mysqlx::Expect::Open::EXPECT_CTX_EMPTY);
      Mysqlx::Expect::Open::Condition *cond = open.mutable_cond()->Add();
      cond->set_condition_key(EXPECT_NO_ERROR);
      cond->set_op(Mysqlx::Expect::Open::Condition::EXPECT_OP_SET);
      return open;
    }

    static Mysqlx::Expect::Open Plain()
    {
      Mysqlx::Expect::Open open;

      open.set_op(Mysqlx::Expect::Open::EXPECT_CTX_EMPTY);
      return open;
    }

    static Mysqlx::Expect::Open Inherit_and_clear_noerror()
    {
      Mysqlx::Expect::Open open = Inherit();

      Mysqlx::Expect::Open::Condition *cond = open.mutable_cond()->Add();
      cond->set_condition_key(EXPECT_NO_ERROR);
      cond->set_op(Mysqlx::Expect::Open::Condition::EXPECT_OP_UNSET);

      return open;
    }

    static Mysqlx::Expect::Open Inherit_and_add_noerror()
    {
      Mysqlx::Expect::Open open = Inherit();

      Mysqlx::Expect::Open::Condition *cond = open.mutable_cond()->Add();
      cond->set_condition_key(EXPECT_NO_ERROR);
      cond->set_op(Mysqlx::Expect::Open::Condition::EXPECT_OP_SET);

      return open;
    }


#define EXPECT_ok_cmd() ASSERT_EQ(success, simulate_instruction(xs, 1, ngs::Error_code()))
#define EXPECT_error_cmd() ASSERT_EQ(ngs::Error_code(1234, "whatever"), simulate_instruction(xs, 2, ngs::Error_code(1234, "whatever")))
#define EXPECT_fail(exp) ASSERT_EQ(ngs::Error_code(ER_X_EXPECT_FAILED, "Expectation failed: " exp), simulate_instruction(xs, 3, ngs::Error_code()))

#define EXPECT_open_ok(msg) ASSERT_EQ(success, simulate_open(xs, msg))
#define EXPECT_open_fail(msg, exp) ASSERT_EQ(ngs::Error_code(ER_X_EXPECT_FAILED, "Expectation failed: " exp), simulate_open(xs, msg))

#define EXPECT_close_ok() ASSERT_EQ(success, simulate_close(xs))
#define EXPECT_close_fail(exp) ASSERT_EQ(ngs::Error_code(ER_X_EXPECT_FAILED, "Expectation failed: " exp), simulate_close(xs))


    TEST(expect, plain)
    {
      Expectation_stack xs;

      // open expect block
      EXPECT_open_ok(Plain());

      // ok command 1
      EXPECT_ok_cmd();
      EXPECT_ok_cmd();
      // error command 2
      EXPECT_error_cmd();

      // subsequent cmds succeed normally
      EXPECT_ok_cmd();
      EXPECT_error_cmd();

      // now close the block
      EXPECT_close_ok();

      EXPECT_ok_cmd();

      // close too much should fail
      EXPECT_EQ(ngs::Error_code(ER_X_EXPECT_NOT_OPEN, "Expect block currently not open"), xs.close());
    }


    TEST(expect, noerror)
    {
      Expectation_stack xs;

      // open expect block
      EXPECT_open_ok(Noerror());

      // ok command 1
      EXPECT_ok_cmd();
      // error command 2
      EXPECT_error_cmd();
      // now everything fails
      EXPECT_fail("no_error");
      EXPECT_fail("no_error");

      // now close the block
      EXPECT_close_fail("no_error");

      // now commands should succeed again
      EXPECT_ok_cmd();
    }


    TEST(expect, noerror_in_noerror)
    {
      Expectation_stack xs;

      // fail in the inner block
      EXPECT_open_ok(Noerror());
      EXPECT_ok_cmd();
      {
        EXPECT_open_ok(Noerror());
        EXPECT_ok_cmd();
        EXPECT_error_cmd();
        EXPECT_fail("no_error");
        EXPECT_close_fail("no_error");
      }
      EXPECT_fail("no_error");
      EXPECT_close_fail("no_error");

      EXPECT_ok_cmd();
      EXPECT_EQ(ngs::Error_code(ER_X_EXPECT_NOT_OPEN, "Expect block currently not open"), xs.close());

      // fail in the outer block
      EXPECT_open_ok(Noerror());
      EXPECT_ok_cmd();
      EXPECT_error_cmd();
      {
        EXPECT_open_fail(Noerror(), "no_error");
        EXPECT_fail("no_error");
        EXPECT_close_fail("no_error");
      }
      EXPECT_fail("no_error");
      EXPECT_close_fail("no_error");

      EXPECT_ok_cmd();
      EXPECT_EQ(ngs::Error_code(ER_X_EXPECT_NOT_OPEN, "Expect block currently not open"), xs.close());

      // fail in inner block again
      EXPECT_open_ok(Noerror());
      EXPECT_ok_cmd();
      {
        EXPECT_open_ok(Inherit());
        EXPECT_ok_cmd();
        EXPECT_error_cmd();
        EXPECT_fail("no_error");
        EXPECT_close_fail("no_error");
      }
      EXPECT_fail("no_error");
      EXPECT_close_fail("no_error");

      EXPECT_ok_cmd();
    }


    TEST(expect, plain_in_noerror)
    {
      Expectation_stack xs;

      // fail in the inner block
      EXPECT_open_ok(Noerror());
      EXPECT_ok_cmd();
      {
        EXPECT_open_ok(Plain());
        EXPECT_ok_cmd();
        EXPECT_error_cmd();
        EXPECT_ok_cmd();
        EXPECT_close_ok();
      }
      EXPECT_close_ok();

      EXPECT_ok_cmd();
      EXPECT_EQ(ngs::Error_code(ER_X_EXPECT_NOT_OPEN, "Expect block currently not open"), xs.close());

      // fail in the outer block
      EXPECT_open_ok(Noerror());
      EXPECT_ok_cmd();
      EXPECT_error_cmd();
      {
        EXPECT_open_fail(Plain(), "no_error");
        EXPECT_fail("no_error");
        EXPECT_close_fail("no_error");
      }
      EXPECT_fail("no_error");
      EXPECT_close_fail("no_error");

      EXPECT_ok_cmd();
      EXPECT_EQ(ngs::Error_code(ER_X_EXPECT_NOT_OPEN, "Expect block currently not open"), xs.close());

      // fail in inner block again
      EXPECT_open_ok(Noerror());
      EXPECT_ok_cmd();
      {
        EXPECT_open_ok(Inherit());
        EXPECT_ok_cmd();
        EXPECT_error_cmd();
        EXPECT_fail("no_error");
        EXPECT_close_fail("no_error");
      }
      EXPECT_fail("no_error");
      EXPECT_close_fail("no_error");

      EXPECT_open_ok(Noerror());
      EXPECT_ok_cmd();
      {
        EXPECT_open_ok(Inherit_and_add_noerror());
        EXPECT_ok_cmd();
        EXPECT_error_cmd();
        EXPECT_fail("no_error");
        EXPECT_close_fail("no_error");
      }
      EXPECT_fail("no_error");
      EXPECT_close_fail("no_error");

      EXPECT_open_ok(Noerror());
      EXPECT_ok_cmd();
      {
        EXPECT_open_ok(Inherit_and_clear_noerror());
        EXPECT_ok_cmd();
        EXPECT_error_cmd();
        EXPECT_ok_cmd();
        EXPECT_close_ok();
      }
      EXPECT_close_ok();

      EXPECT_ok_cmd();
    }


    TEST(expect, noerror_in_plain)
    {
      Expectation_stack xs;

      // fail in the inner block
      EXPECT_open_ok(Plain());
      EXPECT_ok_cmd();
      {
        EXPECT_open_ok(Noerror());
        EXPECT_ok_cmd();
        EXPECT_error_cmd();
        EXPECT_fail("no_error");
        EXPECT_close_fail("no_error");
      }
      EXPECT_ok_cmd();
      EXPECT_close_ok();

      EXPECT_ok_cmd();
      EXPECT_EQ(ngs::Error_code(ER_X_EXPECT_NOT_OPEN, "Expect block currently not open"), xs.close());

      // fail in the outer block
      EXPECT_open_ok(Plain());
      EXPECT_ok_cmd();
      EXPECT_error_cmd();
      {
        EXPECT_open_ok(Noerror());
        EXPECT_ok_cmd();
        EXPECT_close_ok();
      }
      EXPECT_ok_cmd();
      EXPECT_close_ok();

      EXPECT_ok_cmd();
      EXPECT_EQ(ngs::Error_code(ER_X_EXPECT_NOT_OPEN, "Expect block currently not open"), xs.close());

      // fail in inner block again
      EXPECT_open_ok(Plain());
      EXPECT_ok_cmd();
      {
        EXPECT_open_ok(Inherit());
        EXPECT_ok_cmd();
        EXPECT_error_cmd();
        EXPECT_ok_cmd();
        EXPECT_close_ok();
      }
      EXPECT_close_ok();

      EXPECT_open_ok(Plain());
      EXPECT_ok_cmd();
      {
        EXPECT_open_ok(Inherit_and_add_noerror());
        EXPECT_ok_cmd();
        EXPECT_error_cmd();
        EXPECT_fail("no_error");
        EXPECT_close_fail("no_error");
      }
      EXPECT_ok_cmd();
      EXPECT_close_ok();

      EXPECT_ok_cmd();
    }


    TEST(expect, nested_inheriting)
    {
      Expectation_stack xs;
      EXPECT_open_ok(Noerror());
      EXPECT_ok_cmd();
      {
        EXPECT_open_ok(Inherit_and_add_noerror());
        EXPECT_ok_cmd();
        EXPECT_error_cmd();
        EXPECT_fail("no_error");
        EXPECT_close_fail("no_error");
      }
      EXPECT_close_fail("no_error");

      EXPECT_open_ok(Noerror());
      EXPECT_ok_cmd();
      {
        EXPECT_open_ok(Inherit_and_clear_noerror());
        EXPECT_ok_cmd();
        EXPECT_error_cmd();
        EXPECT_ok_cmd();
        EXPECT_close_ok();
      }
      EXPECT_close_ok();

      EXPECT_open_ok(Plain());
      EXPECT_ok_cmd();
      {
        EXPECT_open_ok(Inherit_and_add_noerror());
        EXPECT_ok_cmd();
        EXPECT_error_cmd();
        EXPECT_fail("no_error");
        EXPECT_close_fail("no_error");
      }
      EXPECT_close_ok();

      EXPECT_open_ok(Plain());
      EXPECT_ok_cmd();
      {
        EXPECT_open_ok(Inherit_and_clear_noerror());
        EXPECT_ok_cmd();
        EXPECT_error_cmd();
        EXPECT_ok_cmd();
        EXPECT_close_ok();
      }
      EXPECT_close_ok();
    }

    TEST(expect, invalid)
    {
      {
        Expectation exp;
        EXPECT_EQ(ngs::Error_code(ER_X_EXPECT_BAD_CONDITION, "Unknown condition key"), exp.set(1234, "1"));
      }

      {
        Expectation exp;
        EXPECT_EQ(ngs::Error_code(), exp.set(1, ""));
        EXPECT_EQ(true, exp.fail_on_error());

        EXPECT_EQ(ngs::Error_code(), exp.set(1, "1"));
        EXPECT_EQ(true, exp.fail_on_error());

        EXPECT_EQ(ngs::Error_code(), exp.set(1, "0"));
        EXPECT_FALSE(exp.fail_on_error());

        EXPECT_EQ(ngs::Error_code(ER_X_EXPECT_BAD_CONDITION_VALUE, "Invalid value 'bla' for expectation no_error"), exp.set(1, "bla"));
        EXPECT_FALSE(exp.fail_on_error());
      }
    }


    class Expect_surprise : public xpl::Expect_condition
    {
    public:
      Expect_surprise(bool v) : m_value(v) {}

      virtual Expect_condition *copy()
      {
        Expect_surprise *exp = new Expect_surprise(m_value);
        return exp;
      }

      virtual ngs::Error_code check()
      {
        if (m_value)
          return ngs::Error_code(1, "surprise");
        return ngs::Error_code();
      }

      void set(bool flag)
      {
        m_value = flag;
      }

    private:
      bool m_value;
    };


    TEST(expect, condition)
    {
      Expectation expect;

      Expect_surprise *surp;
      ASSERT_EQ(ngs::Error_code(), expect.check());
      expect.add_condition(surp = new Expect_surprise(false));
      surp->set_key(1234);
      ASSERT_EQ(ngs::Error_code(), expect.check());
      surp->set(true);
      ASSERT_EQ(ngs::Error_code(1, "surprise"), expect.check());
      {
        Expectation copy(expect);

        ASSERT_EQ(ngs::Error_code(1, "surprise"), expect.check());
        ASSERT_EQ(ngs::Error_code(1, "surprise"), copy.check());
        expect.unset(1234);
        ASSERT_EQ(ngs::Error_code(), expect.check());
        ASSERT_EQ(ngs::Error_code(1, "surprise"), copy.check());
      }
    }

  } // namespace test
} // namespace xpl

