/* Copyright (c) 2013, 2014, Oracle and/or its affiliates. All rights reserved.

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

#ifndef TEMPLATE_UTILS_INCLUDED
#define TEMPLATE_UTILS_INCLUDED

/**
  Clears a container, but deletes all objects that the elements point to first.
  @tparam Container of pointers.
 */
template<typename Container_type>
void delete_container_pointers(Container_type &container)
{
  typename Container_type::iterator it1= container.begin();
  typename Container_type::iterator it2= container.end();
  for (; it1 != it2; ++it1)
  {
    delete (*it1);
  }
  container.clear();
}

/**
  Clears a container, but frees all objects that the elements point to first.
  @tparam Container of pointers.
 */
template<typename Container_type>
void my_free_container_pointers(Container_type &container)
{
  typename Container_type::iterator it1= container.begin();
  typename Container_type::iterator it2= container.end();
  for (; it1 != it2; ++it1)
  {
    my_free(*it1);
  }
  container.clear();
}


/**
  Casts from one pointer type, to another, without using
  reinterpret_cast or C-style cast:
    foo *f; bar *b= pointer_cast<bar*>(f);
  This avoids having to do:
    foo *f; bar *b= static_cast<b*>(static_cast<void*>(f));
 */
template<typename T>
inline T pointer_cast(void *p)
{
  return static_cast<T>(p);
}

template<typename T>
inline const T pointer_cast(const void *p)
{
  return static_cast<const T>(p);
}

/**
  Casts from one pointer type to another in a type hierarchy.
  In debug mode, we verify the cast is indeed legal.
 */
template<typename Target, typename Source>
inline Target down_cast(Source arg)
{
  DBUG_ASSERT(NULL != dynamic_cast<Target>(arg));
  return static_cast<Target>(arg);
}


/**
   Sometimes the compiler insists that types be the same and does not do any
   implicit conversion. For example:
   Derived1 *a;
   Derived2 *b; // Derived1 and 2 are children classes of Base
   Base *x= cond ? a : b; // Error, need to force a cast.

   Use:
   Base *x= cond ? implicit_cast<Base*>(a) : implicit_cast<Base*>(b);
   static_cast would work too, but would be less safe (allows any
   pointer-to-pointer conversion, not only up-casts).
*/
template<typename To>
inline To implicit_cast(To x) { return x; }

#endif  // TEMPLATE_UTILS_INCLUDED
