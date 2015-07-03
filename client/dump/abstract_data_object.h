/*
  Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved.

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

#ifndef ABSTRACT_DATA_OBJECT_INCLUDED
#define ABSTRACT_DATA_OBJECT_INCLUDED

#include "i_data_object.h"
#include "my_global.h"
#include <string>

namespace Mysql{
namespace Tools{
namespace Dump{

/**
  Base class for all main DB objects.
 */
class Abstract_data_object : public I_data_object
{
public:
  virtual ~Abstract_data_object();

  /**
    Returns an unique ID of this DB object. This helps progress watching with
    multiple parts of chain during object processing (including queuing).
   */
  uint64 get_id() const;
  /**
    Returns schema in which object is contained.
   */
  std::string get_schema() const;
  /**
    Returns name of object in schema.
   */
  std::string get_name() const;

protected:
  Abstract_data_object(uint64 id, const std::string& name,
    const std::string& schema);

private:
  /**
    An unique ID of this DB object. This helps progress watching with multiple
    parts of chain during object processing (including queuing).
   */
  uint64 m_id;
  /**
    Schema in which object is contained.
    */
  std::string m_schema;
  /**
    Name of object in schema.
   */
  std::string m_name;
};

}
}
}

#endif
