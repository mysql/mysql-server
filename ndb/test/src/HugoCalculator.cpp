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

#include "HugoCalculator.hpp"
#include <NDBT.hpp>

/* *************************************************************
 * HugoCalculator
 *
 *  Comon class for the Hugo test suite, provides the functions 
 *  that is used for calculating values to load in to table and 
 *  also knows how to verify a row that's been read from db 
 *
 * ************************************************************/
HugoCalculator::HugoCalculator(const NdbDictionary::Table& tab) : m_tab(tab) {

  // The "id" column of this table is found in the first integer column
  int i;
  for (i=0; i<m_tab.getNoOfColumns(); i++){ 
    const NdbDictionary::Column* attr = m_tab.getColumn(i);
    if (attr->getType() == NdbDictionary::Column::Unsigned){
      m_idCol = i;
      break;
    }
  }
  
  // The "number of updates" column for this table is found in the last column
  for (i=m_tab.getNoOfColumns()-1; i>=0; i--){
    const NdbDictionary::Column* attr = m_tab.getColumn(i);
    if (attr->getType() == NdbDictionary::Column::Unsigned){
      m_updatesCol = i;
      break;
    }
  }
#if 0
  ndbout << "idCol = " << m_idCol << endl;
  ndbout << "updatesCol = " << m_updatesCol << endl;
#endif
  // Check that idCol is not conflicting with updatesCol
  assert(m_idCol != m_updatesCol && m_idCol != -1 && m_updatesCol != -1);
}

Int32
HugoCalculator::calcValue(int record, 
			  int attrib, 
			  int updates) const {
  const NdbDictionary::Column* attr = m_tab.getColumn(attrib);
  // If this is the "id" column
  if (attrib == m_idCol)
    return record;
 
  // If this is the update column
  if (attrib == m_updatesCol)
    return updates;


  Int32 val;
  if (attr->getPrimaryKey())
    val = record + attrib;
  else
    val = record + attrib + updates;
  return val;
}
#if 0
HugoCalculator::U_Int32 calcValue(int record, int attrib, int updates) const;
HugoCalculator::U_Int64 calcValue(int record, int attrib, int updates) const;
HugoCalculator::Int64 calcValue(int record, int attrib, int updates) const;
HugoCalculator::float calcValue(int record, int attrib, int updates) const;
HugoCalculator::double calcValue(int record, int attrib, int updates) const;
#endif
const char* 
HugoCalculator::calcValue(int record, 
			  int attrib, 
			  int updates, 
			  char* buf) const {
  const char a[26] = {"UAWBORCTDPEFQGNYHISJMKXLZ"};
  const NdbDictionary::Column* attr = m_tab.getColumn(attrib);
  int val = calcValue(record, attrib, updates);

  int len;
  if (attr->getPrimaryKey()){
    // Create a string where val is printed as chars in the beginning
    // of the string, then fill with other chars
    // The string length is set to the same size as the attribute
    len = attr->getLength();
    BaseString::snprintf(buf, len, "%d", val);
    for(int i=strlen(buf); i < len; i++)
      buf[i] = a[((val^i)%25)]; 
  } else{
    
    // Fill buf with some pattern so that we can detect
    // anomalies in the area that we don't fill with chars
    int i;
    for (i = 0; i<attr->getLength(); i++)
      buf[i] = ((i+2) % 255);
    
    // Calculate length of the string to create. We want the string 
    // length to be varied between max and min of this attribute.

    len = val % (attr->getLength() + 1);
    // If len == 0 return NULL if this is a nullable attribute
    if (len == 0){
      if(attr->getNullable() == true)
	return NULL;
      else
	len++;
    }
    for(i=0; i < len; i++)
      buf[i] = a[((val^i)%25)];
    buf[len] = 0;
  }
  return buf;
}

int
HugoCalculator::verifyRowValues(NDBT_ResultRow* const  pRow) const{
  int id, updates;

  id = pRow->attributeStore(m_idCol)->u_32_value();
  updates = pRow->attributeStore(m_updatesCol)->u_32_value();

  // Check the values of each column
  for (int i = 0; i<m_tab.getNoOfColumns(); i++){
    if (i != m_updatesCol && id != m_idCol) {
      
      const NdbDictionary::Column* attr = m_tab.getColumn(i);      
      switch (attr->getType()){
      case NdbDictionary::Column::Char:
      case NdbDictionary::Column::Varchar:
      case NdbDictionary::Column::Binary:
      case NdbDictionary::Column::Varbinary:{
	int result = 0;	  
	char* buf = new char[attr->getLength()+1];
	const char* res = calcValue(id, i, updates, buf);
	if (res == NULL){
	  if (!pRow->attributeStore(i)->isNULL()){
	    g_err << "|- NULL ERROR: expected a NULL but the column was not null" << endl;
	    g_err << "|- The row: \"" << (*pRow) << "\"" << endl;
	    result = -1;
	  }
	} else{
	  if (memcmp(res, pRow->attributeStore(i)->aRef(), pRow->attributeStore(i)->arraySize()) != 0){
	    //	  if (memcmp(res, pRow->attributeStore(i)->aRef(), pRow->attributeStore(i)->getLength()) != 0){
	    g_err << "arraySize(): " 
		   << pRow->attributeStore(i)->arraySize()
		   << ", NdbDict::Column::getLength(): " << attr->getLength()
		   << endl;
	    const char* buf2 = pRow->attributeStore(i)->aRef();
	    for (Uint32 j = 0; j < pRow->attributeStore(i)->arraySize(); j++)
	    {
	      g_err << j << ":" << buf[j] << "[" << buf2[j] << "]";
	      if (buf[j] != buf2[j])
	      {
		g_err << "==>Match failed!";
	      }
	      g_err << endl;
	    }
	    g_err << endl;
	    g_err << "|- Invalid data found in attribute " << i << ": \""
		   << pRow->attributeStore(i)->aRef()
		   << "\" != \"" << res << "\"" << endl
		   << "Length of expected=" << (unsigned)strlen(res) << endl
		   << "Lenght of read="
		   << (unsigned)strlen(pRow->attributeStore(i)->aRef()) << endl;
	    g_err << "|- The row: \"" << (* pRow) << "\"" << endl;
	    result = -1;
	  }
	}
	delete []buf;
	if (result != 0)
	  return result;
      }
	break;
      case NdbDictionary::Column::Int:
      case NdbDictionary::Column::Unsigned:{
	Int32 cval = calcValue(id, i, updates);
	Int32 val = pRow->attributeStore(i)->int32_value();
	if (val != cval){
	  g_err << "|- Invalid data found: \"" << val << "\" != \"" 
		 << cval << "\"" << endl;
	  g_err << "|- The row: \"" << (* pRow) << "\"" << endl;
	  return -1;
	}
	break;
      }
      case NdbDictionary::Column::Bigint:
      case NdbDictionary::Column::Bigunsigned:{
	Uint64 cval = calcValue(id, i, updates);
	Uint64 val = pRow->attributeStore(i)->u_64_value();
	if (val != cval){
	  g_err << "|- Invalid data found: \"" << val << "\" != \"" 
		 << cval << "\"" 
		 << endl;
	  g_err << "|- The row: \"" << (* pRow) << "\"" << endl;
	  return -1;
	}
      }
	break;
      case NdbDictionary::Column::Float:{
	float cval = calcValue(id, i, updates);
	float val = pRow->attributeStore(i)->float_value();
	if (val != cval){
	  g_err << "|- Invalid data found: \"" << val << "\" != \"" 
		 << cval << "\"" << endl;
	  g_err << "|- The row: \"" << (* pRow) << "\"" << endl;
	  return -1;
	}
      }
	break;
      case NdbDictionary::Column::Undefined:
      default:
	assert(false);
	break;
      }
      
    }
  }
  return 0;
}

int
HugoCalculator::getIdValue(NDBT_ResultRow* const pRow) const {
  return pRow->attributeStore(m_idCol)->u_32_value();
}  

int
HugoCalculator::getUpdatesValue(NDBT_ResultRow* const pRow) const {
  return pRow->attributeStore(m_updatesCol)->u_32_value();
}

