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

#ifndef MY_XP_MUTEX_INCLUDED
#define MY_XP_MUTEX_INCLUDED

#include <errno.h>
#include <stdlib.h>

#ifdef _WIN32
#include <windows.h>

typedef CRITICAL_SECTION native_mutex_t;
typedef int              native_mutexattr_t;
#else
#include <pthread.h>
#include <stdio.h>

typedef pthread_mutex_t     native_mutex_t;
typedef pthread_mutexattr_t native_mutexattr_t;
#endif

/**
  @class My_xp_mutex

  Abstract class used to wrap mutex for various platforms.

  A typical use case is:

  @code{.cpp}

  My_xp_mutex mutex= new My_xp_mutex_impl();
  mutex->init(NULL);

  mutex->lock();
  ...
  mutex->unlock();

  @endcode
*/
class My_xp_mutex
{
public:
  /**
    Initialize mutex.

    @param mutex attributes reference
    @return success status
  */

  virtual int init(const native_mutexattr_t *attr)= 0;


  /**
    Destroy mutex.

    @return success status
  */

  virtual int destroy()= 0;


  /**
    Lock mutex.

    @return success status
  */

  virtual int lock()= 0;


  /**
    Trylock mutex.

    @return success status
  */

  virtual int trylock()= 0;


  /**
    Unlock mutex.

    @return success status
  */

  virtual int unlock()= 0;


  /**
    To get native mutex reference.

    @return native mutex pointer
  */

  virtual native_mutex_t *get_native_mutex()= 0;


  virtual ~My_xp_mutex() {}
};

#ifdef _WIN32
class My_xp_mutex_win : public My_xp_mutex
{
private:
  /*
    Disabling the copy constructor and assignment operator.
  */
  My_xp_mutex_win(My_xp_mutex_win const&);
  My_xp_mutex_win& operator=(My_xp_mutex_win const&);
public:
  explicit My_xp_mutex_win();
  virtual ~My_xp_mutex_win();
#else
class My_xp_mutex_pthread : public My_xp_mutex
{
private:
  /*
    Disabling the copy constructor and assignment operator.
  */
  My_xp_mutex_pthread(My_xp_mutex_pthread const&);
  My_xp_mutex_pthread& operator=(My_xp_mutex_pthread const&);
public:
  explicit My_xp_mutex_pthread();
  virtual ~My_xp_mutex_pthread();
#endif
  int init(const native_mutexattr_t *attr);
  int destroy();
  int lock();
  int trylock();
  int unlock();
  native_mutex_t *get_native_mutex();

protected:
  native_mutex_t *m_mutex;
};

#ifdef _WIN32
class My_xp_mutex_impl : public My_xp_mutex_win
#else
class My_xp_mutex_impl : public My_xp_mutex_pthread
#endif
{
public:
  explicit My_xp_mutex_impl() {}
  ~My_xp_mutex_impl() {}
};

class My_xp_mutex_util
{
public:
  /**
    Initialize mutex attributes object

    @param mutex attributes reference
    @return success status
  */

  static int attr_init(native_mutexattr_t *attr);


  /**
    Destroy mutex attributes object

    @param mutex attributes reference
    @return success status
  */

  static int attr_destroy(native_mutexattr_t *attr);
};

#endif // MY_XP_MUTEX_INCLUDED
