/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#include <ndb_global.h>
#include "HugoCalculator.hpp"
#include <NDBT.hpp>

static
Uint32
myRand(Uint64 * seed)
{
  const Uint64 mul= 0x5deece66dull;
  const Uint64 add= 0xb;
  Uint64 loc_result = *seed * mul + add;

  * seed= loc_result;
  return loc_result >> 1;
}

static char base64_table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                             "abcdefghijklmnopqrstuvwxyz"
                             "0123456789+/";

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
  Uint32 j;
  calcValue(record, attrib, updates, (char*)&i, sizeof(i), &j);

  return i;
}
#if 0
HugoCalculator::U_Int32 calcValue(int record, int attrib, int updates) const;
HugoCalculator::U_Int64 calcValue(int record, int attrib, int updates) const;
HugoCalculator::Int64 calcValue(int record, int attrib, int updates) const;
HugoCalculator::float calcValue(int record, int attrib, int updates) const;
HugoCalculator::double calcValue(int record, int attrib, int updates) const;
#endif

static
Uint32
calc_len(Uint32 rvalue, int maxlen)
{
  Uint32 minlen = 25;
  
  if ((rvalue >> 16) < 4096)
    minlen = 15;
  else if ((rvalue >> 16) < 8192)
    minlen = 25;
  else if ((rvalue >> 16) < 16384)
    minlen = 35;
  else
    minlen = 64;

  if (maxlen <= minlen)
    return maxlen;

  return minlen + (rvalue % (maxlen - minlen));
}

const char* 
HugoCalculator::calcValue(int record, 
			  int attrib, 
			  int updates, 
			  char* buf,
			  int len,
			  Uint32 *outlen) const {
  Uint64 seed;
  const NdbDictionary::Column* attr = m_tab.getColumn(attrib);
  Uint32 val;
  * outlen = len;
  do
  {
    if (attrib == m_idCol)
    {
      val= record;
      memcpy(buf, &val, 4);
      return buf;
    }
    
    // If this is the update column
    if (attrib == m_updatesCol)
    {
      val= updates;
      memcpy(buf, &val, 4);
      return buf;
    }
    
    if (attr->getPrimaryKey())
    {
      seed = record + attrib;
    }
    else
    {
      seed = record + attrib + updates;
    }
  } while (0);
  
  val = myRand(&seed);  
  
  if(attr->getNullable() && (((val >> 16) & 255) > 220))
  {
    * outlen = 0;
    return NULL;
  }

  int pos= 0;
  char* dst= buf;
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
  case NdbDictionary::Column::Olddecimal:
  case NdbDictionary::Column::Olddecimalunsigned:
  case NdbDictionary::Column::Decimal:
  case NdbDictionary::Column::Decimalunsigned:
  case NdbDictionary::Column::Binary:
  case NdbDictionary::Column::Datetime:
  case NdbDictionary::Column::Time:
  case NdbDictionary::Column::Date:
  case NdbDictionary::Column::Bit:
    while (len > 4)
    {
      memcpy(buf+pos, &val, 4);
      pos += 4;
      len -= 4;
      val= myRand(&seed);
    }
    
    memcpy(buf+pos, &val, len);
    if(attr->getType() == NdbDictionary::Column::Bit)
    {
      Uint32 bits= attr->getLength();
      Uint32 tmp = bits >> 5;
      Uint32 size = bits & 31;
      ((Uint32*)buf)[tmp] &= ((1 << size) - 1);
    }
    break;
  case NdbDictionary::Column::Varbinary:
  case NdbDictionary::Column::Varchar:
    len = calc_len(myRand(&seed), len - 1);
    assert(len < 256);
    * outlen = len + 1;
    * buf = len;
    dst++;
    goto write_char;
  case NdbDictionary::Column::Longvarchar:
  case NdbDictionary::Column::Longvarbinary:
    len = calc_len(myRand(&seed), len - 2);
    assert(len < 65536);
    * outlen = len + 2;
    int2store(buf, len);
    dst += 2;
write_char:
  case NdbDictionary::Column::Char:
  {
    char* ptr= (char*)&val;
    while(len >= 4)
    {
      len -= 4;
      dst[pos++] = base64_table[ptr[0] & 0x3f];
      dst[pos++] = base64_table[ptr[1] & 0x3f];
      dst[pos++] = base64_table[ptr[2] & 0x3f];
      dst[pos++] = base64_table[ptr[3] & 0x3f];
      val= myRand(&seed);
    }
    
    for(; len; len--, pos++)
      dst[pos] = base64_table[ptr[len] & 0x3f];

    pos--;
    break;
  }
  case NdbDictionary::Column::Blob:
  case NdbDictionary::Column::Undefined:
  case NdbDictionary::Column::Text:
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
      Uint32 len = attr->getSizeInBytes(), real_len;
      char buf[8000];
      const char* res = calcValue(id, i, updates, buf, len, &real_len);
      if (res == NULL){
	if (!pRow->attributeStore(i)->isNULL()){
	  g_err << "|- NULL ERROR: expected a NULL but the column was not null" << endl;
	  g_err << "|- The row: \"" << (*pRow) << "\"" << endl;
	  result = -1;
	}
      } else{
	if (real_len != pRow->attributeStore(i)->get_size_in_bytes())
	{
	  g_err << "|- Invalid data found in attribute " << i << ": \""
		<< "Length of expected=" << real_len << endl
		<< "Lenght of read=" 
		<< pRow->attributeStore(i)->get_size_in_bytes() << endl;
	  result= -1;
	}
	else if (memcmp(res, pRow->attributeStore(i)->aRef(), real_len) != 0)
	{
	  g_err << "Column: " << attr->getName() << endl;
	  const char* buf2 = pRow->attributeStore(i)->aRef();
	  for (Uint32 j = 0; j < len; j++)
	  {
	    g_err << j << ":" << hex << (Uint32)(Uint8)buf[j] << "[" << hex << (Uint32)(Uint8)buf2[j] << "]";
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
		<< pRow->attributeStore(i)->get_size_in_bytes() << endl;
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

