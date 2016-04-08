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

#include <ngs/thread.h>
#include <gtest/gtest.h>

namespace ngs
{

namespace test
{

const int EXPECTED_VALUE_FIRST  = 10;
const int EXPECTED_VALUE_SECOND = 20;
const int EXPECTED_VALUE_THRID  = 30;
const int EXPECTED_VALUE_SET    = 40;
const int EXPECTED_VALUE_SET_EXPECT = 50;

class Ngs_sync_variable : public ::testing::Test
{
public:
  Ngs_sync_variable()
  : m_sut(EXPECTED_VALUE_FIRST), m_thread_ended(false)
  {
  }

  static void* start_routine_set(void *data)
  {
    Ngs_sync_variable *self = (Ngs_sync_variable*)data;
    self->set_value();

    return NULL;
  }

  static void* start_routine_set_and_expect(void *data)
  {
    Ngs_sync_variable *self = (Ngs_sync_variable*)data;
    self->set_value();
    self->m_sut.wait_for(EXPECTED_VALUE_SET_EXPECT);

    return NULL;
  }

  void set_value()
  {
    my_sleep(1);
    m_thread_ended = true;
    m_sut.set(EXPECTED_VALUE_SET);
  }

  void run_thread_set()
  {
    thread_create(0, &m_thr, &start_routine_set, this);
  }

  void run_thread_set_and_expect()
  {
    thread_create(0, &m_thr, &start_routine_set_and_expect, this);
  }

  void join_thread()
  {
    thread_join(&m_thr, NULL);
  }


  Sync_variable<int> m_sut;

  Thread_t m_thr;
  volatile bool m_thread_ended;
};

TEST_F(Ngs_sync_variable, is_returnConstructorInitializedValue)
{
  ASSERT_TRUE(m_sut.is(EXPECTED_VALUE_FIRST));
}

TEST_F(Ngs_sync_variable, is_returnChangedValue)
{
  m_sut.set(EXPECTED_VALUE_SECOND);

  ASSERT_TRUE(m_sut.is(EXPECTED_VALUE_SECOND));
}

TEST_F(Ngs_sync_variable, is_returnChangedValue_afterSetWasCalled)
{
  m_sut.set(EXPECTED_VALUE_SECOND);

  ASSERT_TRUE(m_sut.is(EXPECTED_VALUE_SECOND));
}

TEST_F(Ngs_sync_variable, is_exchangeSuccesses_whenCurrentValueMatches)
{
  ASSERT_TRUE(m_sut.exchange(EXPECTED_VALUE_FIRST, EXPECTED_VALUE_SECOND));
  ASSERT_TRUE(m_sut.is(EXPECTED_VALUE_SECOND));
}

TEST_F(Ngs_sync_variable, is_exchangeFails_whenCurrentValueDoesntMatches)
{
  ASSERT_FALSE(m_sut.exchange(EXPECTED_VALUE_THRID, EXPECTED_VALUE_SECOND));
  ASSERT_FALSE(m_sut.is(EXPECTED_VALUE_SECOND));
  ASSERT_TRUE(m_sut.is(EXPECTED_VALUE_FIRST));
}


TEST_F(Ngs_sync_variable, wait_returnsRightAway_whenCurrentValueMatches)
{
  m_sut.wait_for(EXPECTED_VALUE_FIRST);
}

TEST_F(Ngs_sync_variable, wait_returnsRightAway_whenCurrentValueInArrayMatches)
{
  int VALUES[] = {EXPECTED_VALUE_SECOND, EXPECTED_VALUE_FIRST};
  m_sut.wait_for(VALUES);
}

TEST_F(Ngs_sync_variable, wait_returnsRightAway_whenNewValueMatches)
{
  m_sut.set(EXPECTED_VALUE_SECOND);
  m_sut.wait_for(EXPECTED_VALUE_SECOND);
}

TEST_F(Ngs_sync_variable, set_returnsOldValue)
{
  ASSERT_EQ(EXPECTED_VALUE_FIRST,      m_sut.set_and_return_old(EXPECTED_VALUE_SET_EXPECT));
  ASSERT_EQ(EXPECTED_VALUE_SET_EXPECT, m_sut.set_and_return_old(EXPECTED_VALUE_SECOND));
  ASSERT_EQ(EXPECTED_VALUE_SECOND,     m_sut.set_and_return_old(EXPECTED_VALUE_FIRST));
}

TEST_F(Ngs_sync_variable, wait_returnsRightAway_whenNewCurrentValueInArrayMatches)
{
  int VALUES[] = {EXPECTED_VALUE_SECOND, EXPECTED_VALUE_FIRST};
  m_sut.set(EXPECTED_VALUE_SECOND);
  m_sut.wait_for(VALUES);
}

TEST_F(Ngs_sync_variable, wait_returnsDelayed_whenThreadChangesValueAndItsExpected)
{
  run_thread_set();
  m_sut.wait_for(EXPECTED_VALUE_SET);

  ASSERT_TRUE(m_thread_ended); // Verify that the exit was triggerd by thread

  join_thread();
}

TEST_F(Ngs_sync_variable, wait_returnsDelayed_whenThreadChangesValueAndItsInArrayOfExpectedValues)
{
  run_thread_set_and_expect();
  int VALUES[] = {EXPECTED_VALUE_SET};
  m_sut.wait_for_and_set(VALUES, EXPECTED_VALUE_SET_EXPECT);

  ASSERT_TRUE(m_thread_ended); // Verify that the exit was triggerd by thread
  ASSERT_TRUE(m_sut.is(EXPECTED_VALUE_SET_EXPECT));

  join_thread();
}


} // namespace test

} // namespace xpl
