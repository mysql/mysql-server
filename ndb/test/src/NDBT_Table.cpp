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

#include <NDBT_Table.hpp>
#include <NdbTimer.hpp>
#include <NDBT.hpp>

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

