/* Copyright (c) 2014, 2017, Oracle and/or its affiliates. All rights reserved.

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

#include "dd/string_type.h"  // dd::String_type XXX: temporary, debug-only

namespace dd {

///////////////////////////////////////////////////////////////////////////

namespace cache {
  class Storage_adapter;
  class Dictionary_client;
}

///////////////////////////////////////////////////////////////////////////

/**
  Base class for all data dictionary objects.

  @note This class may be inherited along different paths
        for some subclasses due to the diamond shaped
        inheritance hierarchy; thus, direct subclasses
        must inherit this class virtually.
*/

class Weak_object
{
public:
  // XXX: temporary, debug-only.
  virtual void debug_print(String_type &outb) const = 0;

public:
  Weak_object()
  {}

  virtual ~Weak_object()
  { }

private:
  virtual class Weak_object_impl *impl() = 0;
  virtual const class Weak_object_impl *impl() const= 0;
  friend class cache::Storage_adapter;
  friend class Dictionary_object_table_impl;
  friend class cache::Dictionary_client;
};

///////////////////////////////////////////////////////////////////////////

/** Pretty-printer of data dictionary objects */
struct debug_printer : public String_type
{
	/** Constructor
	@param o	object to pretty-print */
	explicit debug_printer(const dd::Weak_object& o) : String_type()
	{
		o.debug_print(*this);
	}
};

///////////////////////////////////////////////////////////////////////////

}

#endif // DD__WEAK_OBJECT_INCLUDED
