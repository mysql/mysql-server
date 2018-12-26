/* Copyright (c) 2015, 2016, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef MY_XP_COND_INCLUDED
#define MY_XP_COND_INCLUDED

#include <xplatform/my_xp_mutex.h>
#include <xplatform/my_xp_util.h>
#include <time.h>
#include <errno.h>
#include <stdlib.h>

#ifdef _WIN32
typedef CONDITION_VARIABLE native_cond_t;
#else
#include <pthread.h>
#include <stdio.h>

typedef pthread_cond_t native_cond_t;
#endif

/**
  @class My_xp_cond

  Abstract class used to wrap condition for various platforms.

  A typical use case of cond is:

  @code{.cpp}

  My_xp_cond cond= new My_xp_cond_impl();
  cond->init();

  cond->signal();

  @endcode
 */
class My_xp_cond
{
public:
  /**
    Initialize cond.

    @return success status
  */

  virtual int init()= 0;


  /**
    Destroy cond.

    @return success status
  */

  virtual int destroy()= 0;


  /**
    Wait for cond to be signaled during some time before unlocking mutex.

    @param mutex to unlock
    @param time to wait
    @return success status
  */

  virtual int timed_wait(native_mutex_t *mutex,
                         const struct timespec *abstime)= 0;


  /**
    Wait for cond to be signaled to unlock mutex.

    @param mutex to unlock
    @return success status
  */

  virtual int wait(native_mutex_t *mutex)= 0;


  /**
    Signal cond.

    @return success status
  */

  virtual int signal()= 0;


  /**
    Broadcast cond.

    @return success status
  */

  virtual int broadcast()= 0;


  /**
    Get reference to native cond.

    @return native cond
  */

  virtual native_cond_t *get_native_cond()= 0;

  virtual ~My_xp_cond() {}
};

#ifdef _WIN32
class My_xp_cond_win : public My_xp_cond
{
private:
  static DWORD My_xp_cond_win::get_milliseconds(const struct timespec *abstime);
  /*
    Disabling the copy constructor and assignment operator.
  */
  My_xp_cond_win(My_xp_cond_win const&);
  My_xp_cond_win& operator=(My_xp_cond_win const&);
public:
  explicit My_xp_cond_win();
  virtual ~My_xp_cond_win();
#else
class My_xp_cond_pthread : public My_xp_cond
{
private:
  /*
    Disabling the copy constructor and assignment operator.
  */
  My_xp_cond_pthread(My_xp_cond_pthread const&);
  My_xp_cond_pthread& operator=(My_xp_cond_pthread const&);
public:
  explicit My_xp_cond_pthread();
  virtual ~My_xp_cond_pthread();
#endif
  int init();
  int destroy();
  int timed_wait(native_mutex_t *mutex, const struct timespec *abstime);
  int wait(native_mutex_t *mutex);
  int signal();
  int broadcast();
  native_cond_t *get_native_cond();

protected:
  native_cond_t *m_cond;
};

#ifdef _WIN32
class My_xp_cond_impl : public My_xp_cond_win
#else
class My_xp_cond_impl : public My_xp_cond_pthread
#endif
{
public:
  explicit My_xp_cond_impl() {}
  ~My_xp_cond_impl() {}
};

#endif // MY_XP_COND_INCLUDED
