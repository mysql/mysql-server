/* Copyright (c) 2014, 2015 Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */

#ifndef DD__WEAK_OBJECT_INCLUDED
#define DD__WEAK_OBJECT_INCLUDED

#include "my_global.h"

#include <string> // XXX: temporary, debug-only

namespace dd {

///////////////////////////////////////////////////////////////////////////

namespace cache {
  class Storage_adapter;
}

///////////////////////////////////////////////////////////////////////////

/// Base class for all data dictionary objects.
class Weak_object
{
public:
  // XXX: temporary, debug-only.
  virtual void debug_print(std::string &outb) const = 0;

public:
  Weak_object()
  {}

  virtual ~Weak_object()
  { }

#ifndef DBUG_OFF
  // In order to implement cloning for unit testing we must enable
  // copy construcion from sub-classes
protected:
#else
private:
#endif
  /**
    In order to enable DD object copying, we need to implement
    copy constructors and assignment operators for all the DD object
    implementation.  However with current state, we do not have a
    good reason to enable DD object copying in DD framework. Hence,
    we hides copy constructor and assignment operator for Weak_object
    class, which is base class for all the DD objects.
  */
  Weak_object(const Weak_object &weak_object) {}
  Weak_object &operator=(const Weak_object &weak_object);

private:
  virtual class Weak_object_impl *impl() = 0;
  virtual const class Weak_object_impl *impl() const= 0;
  friend class cache::Storage_adapter;
  friend class Dictionary_object_table_impl;
};

///////////////////////////////////////////////////////////////////////////

/** Pretty-printer of data dictionary objects */
struct debug_printer : public std::string
{
	/** Constructor
	@param o	object to pretty-print */
	explicit debug_printer(const dd::Weak_object& o) : std::string()
	{
		o.debug_print(*this);
	}
};

///////////////////////////////////////////////////////////////////////////

}

#endif // DD__WEAK_OBJECT_INCLUDED
