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
#include <Base64.hpp>

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
    if (attr->getType() == NdbDictionary::Column::Unsigned && 
	!attr->getPrimaryKey()){
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
  
  Int32 i;
  calcValue(record, attrib, updates, (char*)&i, sizeof(i));

  return i;
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
			  char* buf,
			  int len) const {

  const NdbDictionary::Column* attr = m_tab.getColumn(attrib);
  Uint32 val;
  do
  {
    if (attrib == m_idCol)
    {
      *((Uint32*)buf)= record;
      return buf;
    }
    
    // If this is the update column
    if (attrib == m_updatesCol)
    {
      *((Uint32*)buf)= updates;
      return buf;
    }
    
    if (attr->getPrimaryKey())
    {
      srand(record + attrib + updates);
      val = (record + attrib);
    }
    else
    {
      srand(record + attrib + updates);
      val = rand();
    }
  } while (0);
  
  if(attr->getNullable() && (((val >> 16) & 255) > 220))
    return NULL;
  
  memcpy(buf, &val, (len > 4 ? 4 : len));
  int pos= 4;
  while(pos + 4 < len)
  {
    val= rand();
    memcpy(buf+pos, &val, 4);
    pos++;
  }

  if(pos < len)
  { 
    val= rand();
    memcpy(buf+pos, &val, (len - pos));
  }

  switch(attr->getType()){
  case NdbDictionary::Column::Tinyint:
  case NdbDictionary::Column::Tinyunsigned:
  case NdbDictionary::Column::Smallint:
  case NdbDictionary::Column::Smallunsigned:
  case NdbDictionary::Column::Mediumint:
  case NdbDictionary::Column::Mediumunsigned:
  case NdbDictionary::Column::Int:
  case NdbDictionary::Column::Unsigned:
  case NdbDictionary::Column::Bigint:
  case NdbDictionary::Column::Bigunsigned:
  case NdbDictionary::Column::Float:
  case NdbDictionary::Column::Double:
  case NdbDictionary::Column::Decimal:
  case NdbDictionary::Column::Binary:
  case NdbDictionary::Column::Datetime:
  case NdbDictionary::Column::Time:
  case NdbDictionary::Column::Date:
    break;
  case NdbDictionary::Column::Bit:
  {
    Uint32 bits= attr->getLength();
    Uint32 tmp = bits >> 5;
    Uint32 size = bits & 31;
    ((Uint32*)buf)[tmp] &= ((1 << size) - 1);
    break;
  }
  case NdbDictionary::Column::Varbinary:
  case NdbDictionary::Column::Varchar:
  case NdbDictionary::Column::Text:
  case NdbDictionary::Column::Char:
  case NdbDictionary::Column::Longvarchar:
  case NdbDictionary::Column::Longvarbinary:
  {
    BaseString tmp;
    base64_encode(buf, len, tmp);
    memcpy(buf, tmp.c_str(), len);
    break;
  }
  case NdbDictionary::Column::Blob:
  case NdbDictionary::Column::Undefined:
    abort();
    break;
  }

  
  return buf;
} 

int
HugoCalculator::verifyRowValues(NDBT_ResultRow* const  pRow) const{
  int id, updates;

  id = pRow->attributeStore(m_idCol)->u_32_value();
  updates = pRow->attributeStore(m_updatesCol)->u_32_value();
  int result = 0;	  
  
  // Check the values of each column
  for (int i = 0; i<m_tab.getNoOfColumns(); i++){
    if (i != m_updatesCol && id != m_idCol) {
      const NdbDictionary::Column* attr = m_tab.getColumn(i);      
      Uint32 len = attr->getSizeInBytes();
      char buf[8000];
      const char* res = calcValue(id, i, updates, buf, len);
      if (res == NULL){
	if (!pRow->attributeStore(i)->isNULL()){
	  g_err << "|- NULL ERROR: expected a NULL but the column was not null" << endl;
	  g_err << "|- The row: \"" << (*pRow) << "\"" << endl;
	  result = -1;
	}
      } else{
	if (memcmp(res, pRow->attributeStore(i)->aRef(), len) != 0){
	  //	  if (memcmp(res, pRow->attributeStore(i)->aRef(), pRow->attributeStore(i)->getLength()) != 0){
	  g_err << "Column: " << attr->getName() << endl;
	  const char* buf2 = pRow->attributeStore(i)->aRef();
	  for (Uint32 j = 0; j < len; j++)
	  {
	    g_err << j << ":" << hex << (int)buf[j] << "[" << hex << (int)buf2[j] << "]";
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
    }
  }
  return result;
}

int
HugoCalculator::getIdValue(NDBT_ResultRow* const pRow) const {
  return pRow->attributeStore(m_idCol)->u_32_value();
}  

int
HugoCalculator::getUpdatesValue(NDBT_ResultRow* const pRow) const {
  return pRow->attributeStore(m_updatesCol)->u_32_value();
}

