/*
   Copyright (c) 2014, 2015 Oracle and/or its affiliates. All rights reserved.

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

#ifndef I_CALLABLE_INCLUDED
#define I_CALLABLE_INCLUDED

namespace Mysql{

/**
  Interface for functors.
  I_callable and all implementations should be removed once we have boost::function<> or std::function<>
*/
template<typename T_result, typename T_arg> class I_callable
{
public:
  virtual ~I_callable()
  {}
  /**
    Function call operator. Invokes callback.
  */
  virtual T_result operator()(T_arg argment) const= 0;
};

}

#endif
