/* Copyright (c) 2015, 2017, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef MY_XP_THREAD_INCLUDED
#define MY_XP_THREAD_INCLUDED

#ifndef XCOM_STANDALONE

#include "my_thread.h"
#include "mysql/psi/psi_thread.h"
#include "my_sys.h"

typedef my_thread_t        native_thread_t;
typedef my_thread_handle   native_thread_handle;
typedef my_thread_attr_t   native_thread_attr_t;
typedef my_start_routine   native_start_routine;

#define NATIVE_THREAD_CREATE_DETACHED MY_THREAD_CREATE_DETACHED
#define NATIVE_THREAD_CREATE_JOINABLE MY_THREAD_CREATE_JOINABLE
#endif

#include "plugin/group_replication/libmysqlgcs/include/mysql/gcs/xplatform/my_xp_cond.h"

/**
  @class My_xp_thread

  Abstract class used to wrap mutex for various platforms.

  A typical use case is:

  @code{.cpp}

  My_xp_thread *thread= new My_xp_thread_impl();
  thread->create(key, NULL, &function, &args);

  void *result;
  thread->join(&result);

  @endcode
*/
class My_xp_thread
{
public:
  /**
    Creates thread.

    @param key thread instrumentation key
    @param attr thread attributes
    @param func routine function
    @param arg function parameters
    @return success status
  */

  virtual int create(PSI_thread_key key, const native_thread_attr_t *attr,
                     native_start_routine func, void *arg)= 0;


  /**
    Creates a detached thread.

    @param key thread instrumentation key
    @param attr thread attributes
    @param func routine function
    @param arg function parameters
    @return success status
  */

  virtual int create_detached(PSI_thread_key key,
                              native_thread_attr_t *attr,
                              native_start_routine func,
                              void *arg)= 0;


  /**
    Suspend invoking thread until this thread terminates.

    @param value_ptr pointer for a placeholder for the terminating thread status
    @return success status
  */

  virtual int join(void **value_ptr)= 0;


  /**
    Cancel this thread.

    @return success status
  */

  virtual int cancel()= 0;


  /**
    Retrieves native thread reference

    @return native thread pointer
  */

  virtual native_thread_t *get_native_thread()= 0;

  virtual ~My_xp_thread() {}
};


#ifndef XCOM_STANDALONE
class My_xp_thread_server : public My_xp_thread
{
public:
  explicit My_xp_thread_server();
  virtual ~My_xp_thread_server();

  int create(PSI_thread_key key, const native_thread_attr_t *attr,
             native_start_routine func, void *arg);
  int create_detached(PSI_thread_key key, native_thread_attr_t *attr,
                      native_start_routine func, void *arg);
  int join(void **value_ptr);
  int cancel();
  native_thread_t *get_native_thread();

protected:
  native_thread_handle *m_thread_handle;
};
#endif


#ifndef XCOM_STANDALONE
class My_xp_thread_impl : public My_xp_thread_server
#endif
{
public:
  explicit My_xp_thread_impl() {}
  ~My_xp_thread_impl() {}
};


class My_xp_thread_util
{
public:
  /**
    Terminate invoking thread.

    @param value_ptr thread exit value pointer
  */

  static void exit(void *value_ptr);


  /**
    Initialize thread attributes object.

    @param attr thread attributes
    @return success status
  */

  static int attr_init(native_thread_attr_t *attr);


  /**
    Destroy thread attributes object.

    @param attr thread attributes
    @return success status
  */

  static int attr_destroy(native_thread_attr_t *attr);


  /**
    Retrieve current thread id.

    @return current thread id
  */

  static native_thread_t self();


  /**
    Compares two thread identifiers.

    @param t1 identifier of one thread
    @param t2 identifier of another thread
    @retval 0 if ids are different
    @retval some other value if ids are equal
  */

  static int equal(native_thread_t t1, native_thread_t t2);


  /**
    Sets the stack size attribute of the thread attributes object referred
    to by attr to the value specified in stacksize.

    @param attr thread attributes
    @param stacksize new attribute stack size
    @retval 0 on success
    @retval nonzero error number, on error
  */

  static int attr_setstacksize(native_thread_attr_t *attr, size_t stacksize);


  /**
    Returns the stack size attribute of the thread attributes object referred
    to by attr in the buffer pointed to by stacksize.

    @param attr thread attributes
    @param stacksize pointer to attribute stack size returning placeholder
    @retval 0 on success
    @retval nonzero error number, on error
  */

  static int attr_getstacksize(native_thread_attr_t *attr, size_t *stacksize);


  /**
    Sets the detach state attribute of the thread attributes object referred
    to by attr to the value specified in detachstate.

    @param attr thread attributes
    @param detachstate determines if the thread is to be created in a joinable
    (MY_THREAD_CREATE_JOINABLE) or a detached state (MY_THREAD_CREATE_DETACHED)
    @retval 0 on success
    @retval nonzero error number, on error
  */

  static int attr_setdetachstate(native_thread_attr_t *attr, int detachstate);


  /**
    Causes the calling thread to relinquish the CPU, and to be moved to the
    end of the queue and another thread gets to run.
  */

  static void yield();

};

#endif // MY_XP_THREAD_INCLUDED
