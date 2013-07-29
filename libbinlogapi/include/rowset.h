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
#ifndef ROWSET_INCLUDED
#define	ROWSET_INCLUDED

#include "field_iterator.h"
#include "resultset_iterator.h"

using namespace mysql;

namespace mysql {

class Row_event;
class Table_map_event;

class Row_event_set
{
public:
    typedef Row_event_iterator<Row_of_fields > iterator;
    typedef Row_event_iterator<Row_of_fields const > const_iterator;

    Row_event_set(Row_event *arg1, Table_map_event *arg2)
    {
      source(arg1, arg2);
    }

    iterator begin()
    {
      return iterator(m_row_event, m_table_map_event);
    }
    iterator end()
    {
      return iterator();
    }
    const_iterator begin() const
    {
      return const_iterator(m_row_event, m_table_map_event);
    }
    const_iterator end() const
    {
      return const_iterator();
    }

private:
    void source(Row_event *arg1, Table_map_event *arg2)
    {
      m_row_event= arg1;
      m_table_map_event= arg2;
    }
    Row_event *m_row_event;
    Table_map_event *m_table_map_event;
};

}
#endif	/* ROWSET_INCLUDED */
