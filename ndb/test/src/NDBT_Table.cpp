/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#include "NDBT_Table.hpp"
#include <NdbTimer.hpp>
#include <NDBT.hpp>


class NdbOut& 
operator <<(class NdbOut& ndbout, const NDBT_Attribute & attr){

  NdbDictionary::Column::Type type = attr.getType();
  bool key = attr.getPrimaryKey();
  bool null = attr.getNullable();

  ndbout << attr.getName() << " ";
  char tmp[100];
  if(attr.getLength() != 1)
    snprintf(tmp, 100," [%d]", attr.getLength());
  else
    tmp[0] = 0;
  
  switch(type){
  case NdbDictionary::Column::Tinyint:
    ndbout << "Tinyint" << tmp;
    break;
  case NdbDictionary::Column::Tinyunsigned:
    ndbout << "Tinyunsigned" << tmp;
    break;
  case NdbDictionary::Column::Smallint:
    ndbout << "Smallint" << tmp;
    break;
  case NdbDictionary::Column::Smallunsigned:
    ndbout << "Smallunsigned" << tmp;
    break;
  case NdbDictionary::Column::Mediumint:
    ndbout << "Mediumint" << tmp;
    break;
  case NdbDictionary::Column::Mediumunsigned:
    ndbout << "Mediumunsigned" << tmp;
    break;
  case NdbDictionary::Column::Int:
    ndbout << "Int" << tmp;
    break;
  case NdbDictionary::Column::Unsigned:
    ndbout << "Unsigned" << tmp;
    break;
  case NdbDictionary::Column::Bigint:
    ndbout << "Bigint"  << tmp;
    break;
  case NdbDictionary::Column::Bigunsigned:
    ndbout << "Bigunsigned"  << tmp;
    break;
  case NdbDictionary::Column::Float:
    ndbout << "Float" << tmp;
    break;
  case NdbDictionary::Column::Double:
    ndbout << "Double"  << tmp;
    break;
  case NdbDictionary::Column::Decimal:
    ndbout << "Decimal(" 
	   << attr.getScale() << ", " << attr.getPrecision() << ")"
	   << tmp;
    break;
  case NdbDictionary::Column::Char:
    ndbout << "Char(" << attr.getLength() << ")";
    break;
  case NdbDictionary::Column::Varchar:
    ndbout << "Varchar(" << attr.getLength() << ")";
    break;
  case NdbDictionary::Column::Binary:
    ndbout << "Binary(" << attr.getLength() << ")";
    break;
  case NdbDictionary::Column::Varbinary:
    ndbout << "Varbinary(" << attr.getLength() << ")";
    break;
  case NdbDictionary::Column::Datetime:
    ndbout << "Datetime"  << tmp;
    break;
  case NdbDictionary::Column::Timespec:
    ndbout << "Timespec"  << tmp;
    break;
  case NdbDictionary::Column::Blob:
    ndbout << "Blob"  << tmp;
    break;
  case NdbDictionary::Column::Undefined:
    ndbout << "Undefined"  << tmp;
    break;
  default:
    ndbout << "Unknown(" << type << ")";
  }
  
  ndbout << " ";
  if(null){
    ndbout << "NULL";
  } else {
    ndbout << "NOT NULL";
  }
  ndbout << " ";
  
  if(key)
    ndbout << "PRIMARY KEY";
  
  return ndbout;
}

class NdbOut& 
operator <<(class NdbOut& ndbout, const NDBT_Table & tab)
{
  ndbout << "-- " << tab.getName() << " --" << endl;
  
  ndbout << "Version: " <<  tab.getObjectVersion() << endl; 
  ndbout << "Fragment type: " <<  tab.getFragmentType() << endl; 
  ndbout << "K Value: " <<  tab.getKValue()<< endl; 
  ndbout << "Min load factor: " <<  tab.getMinLoadFactor()<< endl;
  ndbout << "Max load factor: " <<  tab.getMaxLoadFactor()<< endl; 
  ndbout << "Temporary table: " <<  (tab.getStoredTable() ? "no" : "yes") << endl;
  ndbout << "Number of attributes: " <<  tab.getNoOfColumns() << endl;
  ndbout << "Number of primary keys: " <<  tab.getNoOfPrimaryKeys() << endl;
  ndbout << "Length of frm data: " << tab.getFrmLength() << endl;


  //<< ((tab.getTupleKey() == TupleId) ? " tupleid" : "") <<endl;
  ndbout << "TableStatus: ";
  switch(tab.getObjectStatus()){
  case NdbDictionary::Object::New:
    ndbout << "New" << endl;
    break;
  case NdbDictionary::Object::Changed:
    ndbout << "Changed" << endl;
    break;
  case NdbDictionary::Object::Retrieved:
    ndbout << "Retrieved" << endl;
    break;
  default:
    ndbout << "Unknown(" << tab.getObjectStatus() << ")" << endl;
  }
  
  ndbout << "-- Attributes -- " << endl;
  int noOfAttributes = tab.getNoOfColumns();
  for(int i = 0; i<noOfAttributes; i++){
    ndbout << (* (const NDBT_Attribute*)tab.getColumn(i)) << endl;
  }
  
  return ndbout;
}

class NdbOut& operator <<(class NdbOut&, const NdbDictionary::Index & idx)
{
  ndbout << idx.getName();
  ndbout << "(";
  for (unsigned i=0; i < idx.getNoOfColumns(); i++)
  {
    const NdbDictionary::Column *col = idx.getColumn(i);
    ndbout << col->getName();
    if (i < idx.getNoOfColumns()-1)
      ndbout << ", ";
  }
  ndbout << ")";
  
  ndbout << " - ";
  switch (idx.getType()) {
  case NdbDictionary::Object::UniqueHashIndex:
    ndbout << "UniqueHashIndex";
    break;
  case NdbDictionary::Object::OrderedIndex:
    ndbout << "OrderedIndex";
    break;
  default:
    ndbout << "Type " << idx.getType();
    break;
  }
  return ndbout;
}

