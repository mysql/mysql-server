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

#ifndef DD__FWD_INCLUDED
#define DD__FWD_INCLUDED

#include "dd/iterator.h"

namespace dd {

///////////////////////////////////////////////////////////////////////////

class Abstract_table;
class Charset;
class Collation;
class Column;
class Column_type_element;
class Foreign_key_element;
class Foreign_key;
class Index_element;
class Index;
class Object_table;
class Partition;
class Partition_index;
class Partition_value;
class Schema;
class Table;
class Tablespace_file;
class Tablespace;
class View;
class View_table;

///////////////////////////////////////////////////////////////////////////

typedef Iterator<Abstract_table>      Abstract_table_iterator;
typedef Iterator<Charset>             Charset_iterator;
typedef Iterator<Collation>           Collation_iterator;
typedef Iterator<Column>              Column_iterator;
typedef Iterator<Column_type_element> Column_type_element_iterator;
typedef Iterator<Foreign_key_element> Foreign_key_element_iterator;
typedef Iterator<Foreign_key>         Foreign_key_iterator;
typedef Iterator<Index_element>       Index_element_iterator;
typedef Iterator<Index>               Index_iterator;
typedef Iterator<Partition>           Partition_iterator;
typedef Iterator<Partition_index>     Partition_index_iterator;
typedef Iterator<Partition_value>     Partition_value_iterator;
typedef Iterator<Schema>              Schema_iterator;
typedef Iterator<Table>               Table_iterator;
typedef Iterator<Tablespace_file>     Tablespace_file_iterator;
typedef Iterator<Tablespace>          Tablespace_iterator;
typedef Iterator<View>                View_iterator;
typedef Iterator<View_table>          View_table_iterator;

///////////////////////////////////////////////////////////////////////////

typedef Iterator<const Abstract_table>      Abstract_table_const_iterator;
typedef Iterator<const Charset>             Charset_const_iterator;
typedef Iterator<const Collation>           Collation_const_iterator;
typedef Iterator<const Column>              Column_const_iterator;
typedef Iterator<const Column_type_element> Column_type_element_const_iterator;
typedef Iterator<const Foreign_key_element> Foreign_key_element_const_iterator;
typedef Iterator<const Foreign_key>         Foreign_key_const_iterator;
typedef Iterator<const Index_element>       Index_element_const_iterator;
typedef Iterator<const Index>               Index_const_iterator;
typedef Iterator<const Partition>           Partition_const_iterator;
typedef Iterator<const Partition_index>     Partition_index_const_iterator;
typedef Iterator<const Partition_value>     Partition_value_const_iterator;
typedef Iterator<const Schema>              Schema_const_iterator;
typedef Iterator<const Table>               Table_const_iterator;
typedef Iterator<const Tablespace_file>     Tablespace_file_const_iterator;
typedef Iterator<const Tablespace>          Tablespace_const_iterator;
typedef Iterator<const View>                View_const_iterator;
typedef Iterator<const View_table>          View_table_const_iterator;

///////////////////////////////////////////////////////////////////////////

}

#endif // DD__FWD_INCLUDED
