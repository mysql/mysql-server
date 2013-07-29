/*
Copyright (c) 2003, 2011, 2013, Oracle and/or its affiliates. All rights
reserved.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; version 2 of
the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
02110-1301  USA
*/
#include <vector>

#include "row_of_fields.h"
#include "value.h"
#include <stdexcept>

using namespace mysql;

Row_of_fields& Row_of_fields::operator=(const Row_of_fields &right)
{
  if (size() != right.size())
    throw std::length_error("Row dimension doesn't match.");
  int i= 0;
  Row_of_fields::const_iterator it= right.begin();

  for(; it != right.end() ; it++ )
  {
    this->assign(++i, *it);
  }
  return *this;
}

Row_of_fields& Row_of_fields::operator=(Row_of_fields &right)
{
  if (size() != right.size())
    throw std::length_error("Row dimension doesn't match.");
  int i= 0;
  Row_of_fields::iterator it= right.begin();

  for(; it != right.end() ; it++ )
  {
    this->assign(++i, *it);
  }
  return *this;
}
