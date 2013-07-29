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

#ifndef RESULTSET_ITERATOR_INCLUDED
#define	RESULTSET_ITERATOR_INCLUDED

#include "value.h"
#include "rowset.h"
#include "row_of_fields.h"
#include <iostream>

using namespace mysql;

namespace mysql
{

template <class T>
class Result_set_iterator;

class Result_set
{
public:
    typedef Result_set_iterator<Row_of_fields > iterator;
    typedef Result_set_iterator<Row_of_fields const > const_iterator;

    Result_set(MYSQL *mysql)
    : m_mysql(mysql)
    {
    }
    iterator begin();
    iterator end();
    const_iterator begin() const;
    const_iterator end() const;

private:
    friend class Result_set_iterator<Row_of_fields >;
    friend class Result_set_iterator<Row_of_fields const>;
    int m_row_count;
    std::vector<Row_of_fields > m_rows;
    MYSQL *m_mysql;
    /**
     * The number of fields in the field packets block
     */
    uint64_t m_field_count;
};

template <class Iterator_value_type >
class Result_set_iterator :public std::iterator<std::forward_iterator_tag,
                                                Iterator_value_type>
{
public:
    Result_set_iterator() : m_feeder(0), m_current_row(-1)
    {}

    explicit Result_set_iterator(Result_set *feeder) : m_feeder(feeder),
      m_current_row(-1)
    {
      increment();
    }

    Iterator_value_type operator*()
    {
      return m_feeder->m_rows[m_current_row];
    }


    void operator++()
    {
      if (++m_current_row >= m_feeder->m_row_count)
        m_current_row= -1;
    }


    void operator++(int)
    {
      if (++m_current_row >= m_feeder->m_row_count)
        m_current_row= -1;
    }


    bool operator!=(const Result_set_iterator& other) const
    {
      if (other.m_feeder == 0 && m_feeder == 0)
        return false;

      if (other.m_feeder == 0)
        return m_current_row != -1;

      if (m_feeder == 0)
        return other.m_current_row != -1;

      if (other.m_feeder->m_field_count != m_feeder->m_field_count)
        return true;

      Iterator_value_type *row1= &m_feeder->m_rows[m_current_row];
      Iterator_value_type *row2= &other.m_feeder->m_rows[m_current_row];
      for (unsigned int i= 0; i< m_feeder->m_field_count; ++i)
      {
        Value val1= row1->at(i);
        Value val2= row2->at(i);
        if (val1 != val2)
          return true;
      }
      return false;
    }

 private:
    void increment()
    {
      if (++m_current_row >= m_feeder->m_row_count)
        m_current_row= -1;
    }

    Result_set *m_feeder;
    int m_current_row;
};

} // end namespace mysql



#endif	/* RESULTSET_ITERATOR_INCLUDED */
