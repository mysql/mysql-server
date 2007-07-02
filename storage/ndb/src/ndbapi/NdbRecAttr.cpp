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
#include <NdbOut.hpp>
#include <NdbRecAttr.hpp>
#include <NdbBlob.hpp>
#include "NdbDictionaryImpl.hpp"
#include <NdbTCP.h>

NdbRecAttr::NdbRecAttr(Ndb*)
{
  theStorageX = 0;
  init();
}

NdbRecAttr::~NdbRecAttr()
{
  release();
}

int
NdbRecAttr::setup(const class NdbDictionary::Column* col, char* aValue)
{
  return setup(&(col->m_impl), aValue);
}

int
NdbRecAttr::setup(const NdbColumnImpl* anAttrInfo, char* aValue)
{
  Uint32 tAttrSize = anAttrInfo->m_attrSize;
  Uint32 tArraySize = anAttrInfo->m_arraySize;
  Uint32 tAttrByteSize = tAttrSize * tArraySize;
  
  m_column = anAttrInfo;

  theAttrId = anAttrInfo->m_attrId;
  m_size_in_bytes = tAttrByteSize;
  theValue = aValue;
  m_getVarValue = NULL; // set in getVarValue() only

  if (theStorageX)
    delete[] theStorageX;
  theStorageX = NULL; // "safety first"
  
  // check alignment to signal data
  // a future version could check alignment per data type as well
  
  if (aValue != NULL && (UintPtr(aValue)&3) == 0 && (tAttrByteSize&3) == 0) {
    theRef = aValue;
    return 0;
  }
  if (tAttrByteSize <= 32) {
    theStorage[0] = 0;
    theStorage[1] = 0;
    theStorage[2] = 0;
    theStorage[3] = 0;
    theRef = theStorage;
    return 0;
  }
  Uint32 tSize = (tAttrByteSize + 7) >> 3;
  Uint64* tRef = new Uint64[tSize];
  if (tRef != NULL) {
    for (Uint32 i = 0; i < tSize; i++) {
      tRef[i] = 0;
    }
    theStorageX = tRef;
    theRef = tRef;
    return 0;
  }
  errno = ENOMEM;
  return -1;
}

void
NdbRecAttr::copyout()
{
  char* tRef = (char*)theRef;
  char* tValue = theValue;
  if (tRef != tValue && tRef != NULL && tValue != NULL) {
    Uint32 n = m_size_in_bytes;
    while (n-- > 0) {
      *tValue++ = *tRef++;
    }
  }
}

NdbRecAttr *
NdbRecAttr::clone() const {
  NdbRecAttr * ret = new NdbRecAttr(0);
  if (ret == NULL)
  {
    errno = ENOMEM;
    return NULL;
  }
  ret->theAttrId = theAttrId;
  ret->m_size_in_bytes = m_size_in_bytes;
  ret->m_column = m_column;
  
  Uint32 n = m_size_in_bytes;
  if(n <= 32){
    ret->theRef = (char*)&ret->theStorage[0];
    ret->theStorageX = 0;
    ret->theValue = 0;
  } else {
    ret->theStorageX = new Uint64[((n + 7) >> 3)];
    if (ret->theStorageX == NULL)
    {
      delete ret;
      errno = ENOMEM;
      return NULL;
    }
    ret->theRef = (char*)ret->theStorageX;    
    ret->theValue = 0;
  }
  memcpy(ret->theRef, theRef, n);
  return ret;
}

bool
NdbRecAttr::receive_data(const Uint32 * data32, Uint32 sz)
{
  const unsigned char* data = (const unsigned char*)data32;
  if(sz)
  {
    if (unlikely(m_getVarValue != NULL)) {
      // ONLY for blob V2 implementation
      assert(m_column->getType() == NdbDictionary::Column::Longvarchar ||
             m_column->getType() == NdbDictionary::Column::Longvarbinary);
      assert(sz >= 2);
      Uint32 len = data[0] + (data[1] << 8);
      assert(len == sz - 2);
      assert(len < (1 << 16));
      *m_getVarValue = len;
      data += 2;
      sz -= 2;
    }
    if(!copyoutRequired())
      memcpy(theRef, data, sz);
    else
      memcpy(theValue, data, sz);
    m_size_in_bytes= sz;
    return true;
  } 
  else 
  {
    return setNULL();
  }
  return false;
}

NdbRecordPrintFormat::NdbRecordPrintFormat()
{
  fields_terminated_by= ";";
  start_array_enclosure= "[";
  end_array_enclosure= "]";
  fields_enclosed_by= "";
  fields_optionally_enclosed_by= "\"";
  lines_terminated_by= "\n";
  hex_prefix= "H'";
  null_string= "[NULL]";
  hex_format= 0;
}
NdbRecordPrintFormat::~NdbRecordPrintFormat() {}
static const NdbRecordPrintFormat default_print_format;

static void
ndbrecattr_print_string(NdbOut& out, const NdbRecordPrintFormat &f,
                        const char *type, bool is_binary,
			const char *aref, unsigned sz)
{
  const unsigned char* ref = (const unsigned char*)aref;
  int i, len, printable= 1;
  // trailing zeroes are not printed
  for (i=sz-1; i >= 0; i--)
    if (ref[i] == 0) sz--;
    else break;
  if (!is_binary)
  {
    // trailing spaces are not printed
    for (i=sz-1; i >= 0; i--)
      if (ref[i] == 32) sz--;
      else break;
  }
  if (is_binary && f.hex_format)
  {
    if (sz == 0)
    {
      out.print("0x0");
      return;
    }
    out.print("0x");
    for (len = 0; len < (int)sz; len++)
      out.print("%02X", (int)ref[len]);
    return;
  }
  if (sz == 0) return; // empty

  for (len=0; len < (int)sz && ref[i] != 0; len++)
    if (printable && !isprint((int)ref[i]))
      printable= 0;

  if (printable)
    out.print("%.*s", len, ref);
  else
  {
    out.print("0x");
    for (i=0; i < len; i++)
      out.print("%02X", (int)ref[i]);
  }
  if (len != (int)sz)
  {
    out.print("[");
    for (i= len+1; ref[i] != 0; i++)
    out.print("%u]",len-i);
    assert((int)sz > i);
    ndbrecattr_print_string(out,f,type,is_binary,aref+i,sz-i);
  }
}

NdbOut&
ndbrecattr_print_formatted(NdbOut& out, const NdbRecAttr &r,
                           const NdbRecordPrintFormat &f)
{
  if (r.isNULL())
  {
    out << f.null_string;
    return out;
  }
  
  const NdbDictionary::Column* c = r.getColumn();
  uint length = c->getLength();
  {
    const char *fields_optionally_enclosed_by;
    if (f.fields_enclosed_by[0] == '\0')
      fields_optionally_enclosed_by=
        f.fields_optionally_enclosed_by;
    else
      fields_optionally_enclosed_by= "";
    out << f.fields_enclosed_by;
    Uint32 j;
    switch(r.getType()){
    case NdbDictionary::Column::Bigunsigned:
      out << r.u_64_value();
      break;
    case NdbDictionary::Column::Bit:
      out << f.hex_prefix << "0x";
      {
        const Uint32 *buf = (Uint32 *)r.aRef();
        int k = (length+31)/32;
        while (k > 0 && (buf[--k] == 0));
        out.print("%X", buf[k]);
        while (k > 0)
          out.print("%.8X", buf[--k]);
      }
      break;
    case NdbDictionary::Column::Unsigned:
      if (length > 1)
        out << f.start_array_enclosure;
      out << *(Uint32*)r.aRef();
      for (j = 1; j < length; j++)
        out << " " << *((Uint32*)r.aRef() + j);
      if (length > 1)
        out << f.end_array_enclosure;
      break;
    case NdbDictionary::Column::Mediumunsigned:
      out << r.u_medium_value();
      break;
    case NdbDictionary::Column::Smallunsigned:
      out << r.u_short_value();
      break;
    case NdbDictionary::Column::Tinyunsigned:
      out << (unsigned) r.u_8_value();
      break;
    case NdbDictionary::Column::Bigint:
      out << r.int64_value();
      break;
    case NdbDictionary::Column::Int:
      out << r.int32_value();
      break;
    case NdbDictionary::Column::Mediumint:
      out << r.medium_value();
      break;
    case NdbDictionary::Column::Smallint:
      out << r.short_value();
      break;
    case NdbDictionary::Column::Tinyint:
      out << (int) r.int8_value();
      break;
    case NdbDictionary::Column::Binary:
      if (!f.hex_format)
        out << fields_optionally_enclosed_by;
      j = r.get_size_in_bytes();
      ndbrecattr_print_string(out,f,"Binary", true, r.aRef(), j);
      if (!f.hex_format)
        out << fields_optionally_enclosed_by;
      break;
    case NdbDictionary::Column::Char:
      out << fields_optionally_enclosed_by;
      j = r.get_size_in_bytes();
      ndbrecattr_print_string(out,f,"Char", false, r.aRef(), j);
      out << fields_optionally_enclosed_by;
      break;
    case NdbDictionary::Column::Varchar:
    {
      out << fields_optionally_enclosed_by;
      unsigned len = *(const unsigned char*)r.aRef();
      ndbrecattr_print_string(out,f,"Varchar", false, r.aRef()+1,len);
      j = length;
      out << fields_optionally_enclosed_by;
    }
    break;
    case NdbDictionary::Column::Varbinary:
    {
      if (!f.hex_format)
        out << fields_optionally_enclosed_by;
      unsigned len = *(const unsigned char*)r.aRef();
      ndbrecattr_print_string(out,f,"Varbinary", true, r.aRef()+1,len);
      j = length;
      if (!f.hex_format)
        out << fields_optionally_enclosed_by;
    }
    break;
    case NdbDictionary::Column::Float:
      out << r.float_value();
      break;
    case NdbDictionary::Column::Double:
      out << r.double_value();
      break;
    case NdbDictionary::Column::Olddecimal:
    {
      short len = 1 + c->getPrecision() + (c->getScale() > 0);
      out.print("%.*s", len, r.aRef());
    }
    break;
    case NdbDictionary::Column::Olddecimalunsigned:
    {
      short len = 0 + c->getPrecision() + (c->getScale() > 0);
      out.print("%.*s", len, r.aRef());
    }
    break;
    case NdbDictionary::Column::Decimal:
    case NdbDictionary::Column::Decimalunsigned:
      goto unknown;   // TODO
      break;
      // for dates cut-and-paste from field.cc
    case NdbDictionary::Column::Datetime:
    {
      ulonglong tmp=r.u_64_value();
      long part1,part2,part3;
      part1=(long) (tmp/LL(1000000));
      part2=(long) (tmp - (ulonglong) part1*LL(1000000));
      char buf[40];
      char* pos=(char*) buf+19;
      *pos--=0;
      *pos--= (char) ('0'+(char) (part2%10)); part2/=10; 
      *pos--= (char) ('0'+(char) (part2%10)); part3= (int) (part2 / 10);
      *pos--= ':';
      *pos--= (char) ('0'+(char) (part3%10)); part3/=10;
      *pos--= (char) ('0'+(char) (part3%10)); part3/=10;
      *pos--= ':';
      *pos--= (char) ('0'+(char) (part3%10)); part3/=10;
      *pos--= (char) ('0'+(char) part3);
      *pos--= '/';
      *pos--= (char) ('0'+(char) (part1%10)); part1/=10;
      *pos--= (char) ('0'+(char) (part1%10)); part1/=10;
      *pos--= '-';
      *pos--= (char) ('0'+(char) (part1%10)); part1/=10;
      *pos--= (char) ('0'+(char) (part1%10)); part3= (int) (part1/10);
      *pos--= '-';
      *pos--= (char) ('0'+(char) (part3%10)); part3/=10;
      *pos--= (char) ('0'+(char) (part3%10)); part3/=10;
      *pos--= (char) ('0'+(char) (part3%10)); part3/=10;
      *pos=(char) ('0'+(char) part3);
      out << buf;
    }
    break;
    case NdbDictionary::Column::Date:
    {
      uint32 tmp=(uint32) uint3korr(r.aRef());
      int part;
      char buf[40];
      char *pos=(char*) buf+10;
      *pos--=0;
      part=(int) (tmp & 31);
      *pos--= (char) ('0'+part%10);
      *pos--= (char) ('0'+part/10);
      *pos--= '-';
      part=(int) (tmp >> 5 & 15);
      *pos--= (char) ('0'+part%10);
      *pos--= (char) ('0'+part/10);
      *pos--= '-';
      part=(int) (tmp >> 9);
      *pos--= (char) ('0'+part%10); part/=10;
      *pos--= (char) ('0'+part%10); part/=10;
      *pos--= (char) ('0'+part%10); part/=10;
      *pos=   (char) ('0'+part);
      out << buf;
    }
    break;
    case NdbDictionary::Column::Time:
    {
      long tmp=(long) sint3korr(r.aRef());
      int hour=(uint) (tmp/10000);
      int minute=(uint) (tmp/100 % 100);
      int second=(uint) (tmp % 100);
      char buf[40];
      sprintf(buf, "%02d:%02d:%02d", hour, minute, second);
      out << buf;
    }
    break;
    case NdbDictionary::Column::Year:
    {
      uint year = 1900 + r.u_8_value();
      char buf[40];
      sprintf(buf, "%04d", year);
      out << buf;
    }
    break;
    case NdbDictionary::Column::Timestamp:
    {
      time_t time = r.u_32_value();
      out << (uint)time;
    }
    break;
    case NdbDictionary::Column::Blob:
    case NdbDictionary::Column::Text:
    {
      NdbBlob::Head head;
      NdbBlob::unpackBlobHead(head, r.aRef(), c->getBlobVersion());
      out << head.length << ":";
      const unsigned char* p = (const unsigned char*)r.aRef() + head.headsize;
      if (r.get_size_in_bytes() < head.headsize)
        out << "***error***"; // really cannot happen
      else {
        unsigned n = r.get_size_in_bytes() - head.headsize;
        for (unsigned k = 0; k < n && k < head.length; k++) {
          if (r.getType() == NdbDictionary::Column::Blob)
            out.print("%02X", (int)p[k]);
          else
            out.print("%c", (int)p[k]);
        }
      }
      j = length;
    }
    break;
    case NdbDictionary::Column::Longvarchar:
    {
      out << fields_optionally_enclosed_by;
      unsigned len = uint2korr(r.aRef());
      ndbrecattr_print_string(out,f,"Longvarchar", false, r.aRef()+2,len);
      j = length;
      out << fields_optionally_enclosed_by;
    }
    break;
    case NdbDictionary::Column::Longvarbinary:
    {
      if (!f.hex_format)
        out << fields_optionally_enclosed_by;
      unsigned len = uint2korr(r.aRef());
      ndbrecattr_print_string(out,f,"Longvarbinary", true, r.aRef()+2,len);
      j = length;
      if (!f.hex_format)
        out << fields_optionally_enclosed_by;
    }
    break;

    case NdbDictionary::Column::Undefined:
    unknown:
    //default: /* no print functions for the rest, just print type */
    out << (int) r.getType();
    j = length;
    if (j > 1)
      out << " " << j << " times";
    break;
    }
    out << f.fields_enclosed_by;
  }

  return out;
}

NdbOut& operator<<(NdbOut& out, const NdbRecAttr &r)
{
  return ndbrecattr_print_formatted(out, r, default_print_format);
}

Int64
NdbRecAttr::int64_value() const 
{
  Int64 val;
  memcpy(&val,theRef,8);
  return val;
}

Uint64
NdbRecAttr::u_64_value() const
{
  Uint64 val;
  memcpy(&val,theRef,8);
  return val;
}

float
NdbRecAttr::float_value() const
{
  float val;
  memcpy(&val,theRef,sizeof(val));
  return val;
}

double
NdbRecAttr::double_value() const
{
  double val;
  memcpy(&val,theRef,sizeof(val));
  return val;
}

Int32
NdbRecAttr::medium_value() const
{
  return sint3korr((unsigned char *)theRef);
}

Uint32
NdbRecAttr::u_medium_value() const
{
  return uint3korr((unsigned char*)theRef);
}
