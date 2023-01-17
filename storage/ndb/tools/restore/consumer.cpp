/*
   Copyright (c) 2004, 2023, Oracle and/or its affiliates.

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

#include "consumer.hpp"

#ifdef USE_MYSQL
int
BackupConsumer::create_table_string(const TableS & table,
				    char * tableName,
				    char *buf){
  int pos = 0;
  int pos2 = 0;
  char buf2[2048];

  pos += sprintf(buf+pos, "%s%s", "CREATE TABLE ",  tableName);
  pos += sprintf(buf+pos, "%s", "(");
  pos2 += sprintf(buf2+pos2, "%s", " primary key(");

  for (int j = 0; j < table.getNoOfAttributes(); j++) 
  {
    const AttributeDesc * desc = table[j];
    //   ndbout << desc->name << ": ";
    pos += sprintf(buf+pos, "%s%s", desc->m_column->getName()," ");
    switch(desc->m_column->getType()){
    case NdbDictionary::Column::Int:
      pos += sprintf(buf+pos, "%s", "int");
      break;
    case NdbDictionary::Column::Unsigned:
      pos += sprintf(buf+pos, "%s", "int unsigned");
      break;
    case NdbDictionary::Column::Float:
      pos += sprintf(buf+pos, "%s", "float");
      break;
    case NdbDictionary::Column::Olddecimal:
    case NdbDictionary::Column::Decimal:
      pos += sprintf(buf+pos, "%s", "decimal");
      break;
    case NdbDictionary::Column::Olddecimalunsigned:
    case NdbDictionary::Column::Decimalunsigned:
      pos += sprintf(buf+pos, "%s", "decimal unsigned");
      break;
    case NdbDictionary::Column::Char:
      pos += sprintf(buf+pos, "%s", "char");
      break;
    case NdbDictionary::Column::Varchar:
      pos += sprintf(buf+pos, "%s", "varchar");
      break;
    case NdbDictionary::Column::Binary:
      pos += sprintf(buf+pos, "%s", "binary");
      break;
    case NdbDictionary::Column::Varbinary:
      pos += sprintf(buf+pos, "%s", "varchar binary");
      break;
    case NdbDictionary::Column::Bigint:
      pos += sprintf(buf+pos, "%s", "bigint");
      break;
    case NdbDictionary::Column::Bigunsigned:
      pos += sprintf(buf+pos, "%s", "bigint unsigned");
      break;
    case NdbDictionary::Column::Double:
      pos += sprintf(buf+pos, "%s", "double");
      break;
    case NdbDictionary::Column::Datetime:
      pos += sprintf(buf+pos, "%s", "datetime");
      break;
    case NdbDictionary::Column::Date:
      pos += sprintf(buf+pos, "%s", "date");
      break;
    case NdbDictionary::Column::Time:
      pos += sprintf(buf+pos, "%s", "time");
      break;
    case NdbDictionary::Column::Undefined:
      //      pos += sprintf(buf+pos, "%s", "varchar binary");
      return -1;
      break;
    default:
      //pos += sprintf(buf+pos, "%s", "varchar binary");
      return -1;
    }
    if (desc->arraySize > 1) {
      int attrSize = desc->arraySize;
      pos += sprintf(buf+pos, "%s%u%s",
		     "(",
		     attrSize,
		     ")");
    }
    if (desc->m_column->getPrimaryKey()) {
      pos += sprintf(buf+pos, "%s", " not null");
      pos2 += sprintf(buf2+pos2, "%s%s", desc->m_column->getName(), ",");
    }
    pos += sprintf(buf+pos, "%s", ",");
  } // for
  pos2--; // remove trailing comma
  pos2 += sprintf(buf2+pos2, "%s", ")");
  //  pos--; // remove trailing comma

  pos += sprintf(buf+pos, "%s", buf2);
  pos += sprintf(buf+pos, "%s", ") type=ndbcluster");
  return 0;
}

#endif // USE_MYSQL
