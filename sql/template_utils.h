/* Copyright (c) 2013, 2014 Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02111-1301  USA */

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

#endif  // TEMPLATE_UTILS_INCLUDED
