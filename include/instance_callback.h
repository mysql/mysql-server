/*
   Copyright (c) 2014, 2016 Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef INSTANCE_CALLBACK_INCLUDED
#define INSTANCE_CALLBACK_INCLUDED

#include "i_callable.h"
#include <cstddef> // NULL

namespace Mysql{

/**
  Instance method based callback.
*/
template<typename T_result, typename T_arg, typename T_class>
  class Instance_callback : public I_callable<T_result, T_arg>
{

  typedef T_result (T_class::*Method)(T_arg);

public:
  /**
    Creates new instance method callback.
  */
  Instance_callback(T_class* instance, Method method);

  /**
    Invokes callback method on instance specified in constructor.
  */
  T_result operator()(T_arg argument) const;

protected:
  T_class* m_instance;
private:
  Method m_method;
};

/**
  Instance method based callback. Owns T_class instance and destroys it when it
  dies.
*/
template<typename T_result, typename T_arg, typename T_class>
  class Instance_callback_own
    : public Instance_callback<T_result, T_arg, T_class>
{
  typedef T_result (T_class::*Method)(T_arg);

public:
  /**
    Creates new instance method callback.
  */
  Instance_callback_own(T_class* instance, Method method);

  ~Instance_callback_own();
};

template<typename T_result, typename T_arg, typename T_class>
  Instance_callback<T_result, T_arg, T_class>::
    Instance_callback(T_class* instance, Method method)
  : m_instance(instance), m_method(method)
{}

template<typename T_result, typename T_arg, typename T_class>
  T_result Instance_callback<T_result, T_arg, T_class>::operator()
    (T_arg argument) const
{
  return (this->m_instance->*this->m_method)(argument);
}

template<typename T_result, typename T_arg, typename T_class>
  Instance_callback_own<T_result, T_arg, T_class>::
    Instance_callback_own(T_class* instance, Method method)
  : Instance_callback<T_result, T_arg, T_class>(instance, method)
{}

template<typename T_result, typename T_arg, typename T_class>
  Instance_callback_own<T_result, T_arg, T_class>::
    ~Instance_callback_own()
{
  delete this->m_instance;
  this->m_instance= NULL;
}

}

#endif
