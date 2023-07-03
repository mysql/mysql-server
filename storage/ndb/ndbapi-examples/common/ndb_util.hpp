/*
   Copyright (c) 2014, 2022, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef NDB_UTIL_HPP
#define NDB_UTIL_HPP

#include <NdbApi.hpp>
#include <string>
#include <sstream>

static const std::string column_type_to_string(NdbDictionary::Column::Type type)
{
  switch (type)
  {
  case NdbDictionary::Column::Undefined:
    return "Undefined";
  case NdbDictionary::Column::Tinyint:
    return "Tinyint";
  case NdbDictionary::Column::Tinyunsigned:
    return "Tinyunsigned";
  case NdbDictionary::Column::Smallint:
    return "Smallint";
  case NdbDictionary::Column::Smallunsigned:
    return "Smallunsigned";
  case NdbDictionary::Column::Mediumint:
    return "Mediumint";
  case NdbDictionary::Column::Mediumunsigned:
    return "Mediumunsigned";
  case NdbDictionary::Column::Int:
    return "Int";
  case NdbDictionary::Column::Unsigned:
    return "Unsigned";
  case NdbDictionary::Column::Bigint:
    return "Bigint";
  case NdbDictionary::Column::Bigunsigned:
    return "Bigunsigned";
  case NdbDictionary::Column::Float:
    return "Float";
  case NdbDictionary::Column::Double:
    return "Double";
  case NdbDictionary::Column::Olddecimal:
    return "Olddecimal";
  case NdbDictionary::Column::Olddecimalunsigned:
    return "Olddecimalunsigned";
  case NdbDictionary::Column::Decimal:
    return "Decimal";
  case NdbDictionary::Column::Decimalunsigned:
    return "Decimalunsigned";
  case NdbDictionary::Column::Char:
    return "Char";
  case NdbDictionary::Column::Varchar:
    return "Varchar";
  case NdbDictionary::Column::Binary:
    return "Binary";
  case NdbDictionary::Column::Varbinary:
    return "Varbinary";
  case NdbDictionary::Column::Datetime:
    return "Datetime";
  case NdbDictionary::Column::Date:
    return "Date";
  case NdbDictionary::Column::Blob:
    return "Blob";
  case NdbDictionary::Column::Text:
    return "Text";
  case NdbDictionary::Column::Bit:
    return "Bit";
  case NdbDictionary::Column::Longvarchar:
    return "Longvarchar";
  case NdbDictionary::Column::Longvarbinary:
    return "Longvarbinary";
  case NdbDictionary::Column::Time:
    return "Time";
  case NdbDictionary::Column::Year:
    return "Year";
  case NdbDictionary::Column::Timestamp:
    return "Timestamp";
  case NdbDictionary::Column::Time2:
    return "Time2";
  case NdbDictionary::Column::Datetime2:
    return "Datetime2";
  case NdbDictionary::Column::Timestamp2:
    return "Timestamp2";
  default:
    {
      std::string str;
      std::stringstream s(str);
      s << "Unknown type: " << type;
      return s.str();
    }
  }
}

#endif
