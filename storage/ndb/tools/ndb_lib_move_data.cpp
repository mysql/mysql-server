/* Copyright (c) 2016, 2018, Oracle and/or its affiliates. All rights reserved.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include <ndb_global.h>
#include "m_ctype.h"
#include <NdbOut.hpp>
#include <NdbApi.hpp>
#include <NDBT.hpp>
#include <ndb_limits.h>
#include <ndb_lib_move_data.hpp>
#include <ndb_rand.h>

#define CHK1(b) \
  if (!(b)) { \
    ret = -1; \
    abort_on_error(); \
    break; \
  }

#define CHK2(b, e) \
  if (!(b)) { \
    set_error_line(__LINE__); \
    set_error_code e; \
    ret = -1; \
    abort_on_error(); \
    break; \
  }

Ndb_move_data::Ndb_move_data()
{
  m_source = 0;
  m_target = 0;
  m_sourceattr = 0;
  m_targetattr = 0;
  m_data = 0;
  m_error_insert = false;
}

Ndb_move_data::~Ndb_move_data()
{
  delete [] m_sourceattr;
  delete [] m_targetattr;
  m_sourceattr = 0;
  m_targetattr = 0;
  release_data();
}

int
Ndb_move_data::init(const NdbDictionary::Table* source,
                    const NdbDictionary::Table* target)
{
  int ret = 0;
  do
  {
    CHK2(source != 0, (Error::InvalidSource, "null source table pointer"));
    CHK2(source->getObjectStatus() == NdbDictionary::Object::Retrieved,
         (Error::InvalidSource, "source table status is not Retrieved"));

    CHK2(target != 0, (Error::InvalidTarget, "null target table pointer"));
    CHK2(target->getObjectStatus() == NdbDictionary::Object::Retrieved,
         (Error::InvalidTarget, "target table status is not Retrieved"));

    m_source = source;
    m_target = target;
  }
  while (0);
  return ret;
}

Ndb_move_data::Attr::Attr()
{
  column = 0;
  name = 0;
  id = -1;
  map_id = -1;
  type = TypeNone;
  size_in_bytes = 0;
  length_bytes = 0;
  data_size = 0;
  pad_char = -1;
  equal = false;
}

void
Ndb_move_data::set_type(Attr& attr, const NdbDictionary::Column* c)
{
  attr.column = c;
  attr.name = c->getName();
  attr.size_in_bytes = c->getSizeInBytes();
  switch (c->getType()) {
  case NdbDictionary::Column::Char:
  case NdbDictionary::Column::Binary:
    attr.type = Attr::TypeArray;
    attr.length_bytes = 0;
    attr.data_size = attr.size_in_bytes;
    if (c->getType() == NdbDictionary::Column::Char)
      attr.pad_char = 0x20;
    else
      attr.pad_char = 0x0;
    break;
  case NdbDictionary::Column::Varchar:
  case NdbDictionary::Column::Varbinary:
    attr.type = Attr::TypeArray;
    attr.length_bytes = 1;
    require(attr.size_in_bytes >= attr.length_bytes);
    attr.data_size = attr.size_in_bytes - attr.length_bytes;
    attr.pad_char = -1;
    break;
  case NdbDictionary::Column::Longvarchar:
  case NdbDictionary::Column::Longvarbinary:
    attr.type = Attr::TypeArray;
    attr.length_bytes = 2;
    require(attr.size_in_bytes >= attr.length_bytes);
    attr.data_size = attr.size_in_bytes - attr.length_bytes;
    attr.pad_char = -1;
    break;
  case NdbDictionary::Column::Text:
  case NdbDictionary::Column::Blob:
    attr.type = Attr::TypeBlob;
    attr.length_bytes = 0;
    break;
  default:
    attr.type = Attr::TypeOther;
    break;
  }
}

Uint32
Ndb_move_data::calc_str_len_truncated(CHARSET_INFO *cs, char *data, Uint32 maxlen)
{
  const char *begin = (const char*) data;
  const char *end= (const char*) (data+maxlen);
  int errors = 0;
  // for multi-byte characters, truncate to last well-formed character before
  // maxlen so that string is not truncated in the middle of a multi-byte char. 
  Uint32 numchars = cs->cset->numchars(cs, begin, end); 
  Uint32 wf_len = cs->cset->well_formed_len(cs, begin, end, numchars, &errors);
  require(wf_len <= maxlen);

  return wf_len;
}
  
int
Ndb_move_data::check_nopk(const Attr& attr1, const Attr& attr2)
{
  int ret = 0;
  const NdbDictionary::Column* c1 = attr1.column;
  const NdbDictionary::Column* c2 = attr2.column;
  do
  {
    CHK2(!c1->getPrimaryKey() && !c2->getPrimaryKey(),
         (Error::UnsupportedConversion,
          "cannot convert column #%d '%s'"
          ": primary key attributes not allowed here",
          1+attr1.id, attr1.name));
  }
  while (0);
  return ret;
}

int
Ndb_move_data::check_promotion(const Attr& attr1, const Attr& attr2)
{
  (void)attr2;
  int ret = 0;
  const Opts& opts = m_opts;
  do
  {
    CHK2(opts.flags & Opts::MD_ATTRIBUTE_PROMOTION,
         (Error::NoPromotionFlag,
          "cannot convert column #%d '%s'"
          ": promote-attributes has not been specified",
          1+attr1.id, attr1.name));
  }
  while (0);
  return ret;
}

int
Ndb_move_data::check_demotion(const Attr& attr1, const Attr& attr2)
{
  (void)attr2;
  int ret = 0;
  const Opts& opts = m_opts;
  do
  {
    CHK2(opts.flags & Opts::MD_ATTRIBUTE_DEMOTION,
         (Error::NoDemotionFlag,
          "cannot convert column #%d '%s'"
          ": demote-attributes has not been specified",
          1+attr1.id, attr1.name));
  }
  while (0);
  return ret;
}

int
Ndb_move_data::check_sizes(const Attr& attr1, const Attr& attr2)
{
  int ret = 0;
  do
  {
    if (attr1.data_size < attr2.data_size)
    {
      CHK1(check_promotion(attr1, attr2) == 0);
    }
    if (attr1.data_size > attr2.data_size)
    {
      CHK1(check_demotion(attr1, attr2) == 0);
    }
  }
  while (0);
  return ret;
}

int
Ndb_move_data::check_unsupported(const Attr& attr1, const Attr& attr2)
{
  (void)attr2;
  int ret = 0;
  do
  {
    CHK2(false,
         (Error::UnsupportedConversion,
          "cannot convert column #%d '%s'"
          ": unimplemented conversion",
          1+attr1.id, attr1.name));
  }
  while (0);
  return ret;
}

int
Ndb_move_data::check_tables()
{
  int ret = 0;
  const Opts& opts = m_opts;
  do
  {
    int attrcount1 = m_source->getNoOfColumns();
    int attrcount2 = m_target->getNoOfColumns();
    m_sourceattr = new Attr [attrcount1];
    m_targetattr = new Attr [attrcount2];

    // set type info, remap columns, check missing

    for (int i1 = 0; i1 < attrcount1; i1++)
    {
      Attr& attr1 = m_sourceattr[i1];
      attr1.id = i1;
      const NdbDictionary::Column* c1 = m_source->getColumn(i1);
      require(c1 != 0);
      set_type(attr1, c1);
      const NdbDictionary::Column* c2 = m_target->getColumn(attr1.name);
      if (c2 == 0)
      {
        CHK2(opts.flags & Opts::MD_EXCLUDE_MISSING_COLUMNS,
             (Error::NoExcludeMissingFlag,
              "cannot convert source to target"
              ": source column #%d '%s' not found in target"
              " and exclude-missing-columns has not been specified",
              1+i1, attr1.name));
      }
      else
      {
        int i2 = c2->getColumnNo();
        require(i2 >= 0 && i2 < attrcount2);
        Attr& attr2 = m_targetattr[i2];
        attr1.map_id = i2;
        require(attr2.map_id == -1);
        attr2.map_id = i1;
      }
    }
    CHK1(ret == 0);

    for (int i2 = 0; i2 < attrcount2; i2++)
    {
      Attr& attr2 = m_targetattr[i2];
      attr2.id = i2;
      const NdbDictionary::Column* c2 = m_target->getColumn(i2);
      require(c2 != 0);
      set_type(attr2, c2);
      const NdbDictionary::Column* c1 = m_source->getColumn(attr2.name);
      if (c1 == 0)
      {
        CHK2(opts.flags & Opts::MD_EXCLUDE_MISSING_COLUMNS,
             (Error::NoExcludeMissingFlag,
              "cannot convert source to target"
              ": target column #%d '%s' not found in source"
              " and exclude-missing-columns has not been specified",
              1+i2, attr2.name));
      }
      else
      {
        int i1 = c1->getColumnNo();
        require(i2 >= 0 && i2 < attrcount2);
        Attr& attr1 = m_sourceattr[i1];
        require(attr1.map_id == i2);
        require(attr2.map_id == i1);
      }
    }
    CHK1(ret == 0);

    // check conversion of non-excluded columns

    for (int i1 = 0; i1 < attrcount1; i1++)
    {
      Attr& attr1 = m_sourceattr[i1];
      int i2 = attr1.map_id;
      if (i2 == -1) // excluded
        continue;
      Attr& attr2 = m_targetattr[i2];

      {
        /* Exclude internal implementation details when comparing */
        NdbDictionary::Column a1ColCopy(*attr1.column);
        NdbDictionary::Column a2ColCopy(*attr2.column);
        
        /* [Non] Dynamic internal storage is irrelevant */ 
        a1ColCopy.setDynamic(false);
        a2ColCopy.setDynamic(false);
        
        if ((attr1.equal = attr2.equal = a1ColCopy.equal(a2ColCopy)))
        {
          continue;
        }
      }

      if (attr1.type == Attr::TypeArray &&
          attr2.type == Attr::TypeBlob)
      {
        CHK1(check_nopk(attr1, attr2) == 0);
        CHK1(check_promotion(attr1, attr2) == 0);
        continue;
      }

      if (attr1.type == Attr::TypeBlob &&
          attr2.type == Attr::TypeArray)
      {
        CHK1(check_nopk(attr1, attr2) == 0);
        CHK1(check_demotion(attr1, attr2) == 0);
        continue;
      }

      if (attr1.type == Attr::TypeArray &&
          attr2.type == Attr::TypeArray)
      {
        CHK1(check_sizes(attr1, attr2) == 0);
        continue;
      }

      if (attr1.type == Attr::TypeBlob &&
          attr2.type == Attr::TypeBlob)
      {
        // TEXT and BLOB conversions
        CHK1(check_sizes(attr1, attr2) == 0);
        continue;
      }

      CHK1(check_unsupported(attr1, attr2) == 0);
    }
    CHK1(ret == 0);
  }
  while (0);
  return ret;
}

Ndb_move_data::Data::Data()
{
  data = 0;
  next = 0;
}

Ndb_move_data::Data::~Data()
{
  delete [] data;
  data = 0;
}

char*
Ndb_move_data::alloc_data(Uint32 n)
{
  Data* d = new Data;
  d->data = new char [n];
  d->next = m_data;
  m_data = d;
  return d->data;
}

void
Ndb_move_data::release_data()
{
  while (m_data != 0)
  {
    Data* d = m_data;
    m_data = d->next;
    delete d;
  }
}

Ndb_move_data::Op::Op()
{
  ndb = 0;
  scantrans = 0;
  scanop = 0;
  updatetrans = 0;
  updateop = 0;
  values = 0;
  buflen = 32 * 1024;
  require(buflen >= (4 * MAX_TUPLE_SIZE_IN_WORDS));
  buf1 = new char [buflen];
  buf2 = new char [buflen];
  rows_in_batch = 0;
  truncated_in_batch = 0;
  end_of_scan = false;
}

Ndb_move_data::Op::~Op()
{
  delete [] values;
  delete [] buf1;
  delete [] buf2;
  values = 0;
  buf1 = 0;
  buf2 = 0;
}

int
Ndb_move_data::start_scan()
{
  int ret = 0;
  Op& op = m_op;
  const int attrcount1 = m_source->getNoOfColumns();
  do
  {
    require(op.scantrans == 0);
    op.scantrans = op.ndb->startTransaction(m_source);
    CHK2(op.scantrans != 0, (op.ndb->getNdbError()));

    op.scanop = op.scantrans->getNdbScanOperation(m_source);
    CHK2(op.scanop != 0, (op.scantrans->getNdbError()));

    NdbOperation::LockMode lm = NdbOperation::LM_Exclusive;
    Uint32 flags = 0;
    CHK2(op.scanop->readTuples(lm, flags) == 0, (op.scanop->getNdbError()));

    require(op.values == 0);
    op.values = new Op::Value [attrcount1];

    for (int i1 = 0; i1 < attrcount1; i1++)
    {
      const Attr& attr1 = m_sourceattr[i1];

      if (attr1.type != Attr::TypeBlob)
      {
        NdbRecAttr* ra = op.scanop->getValue(i1);
        CHK2(ra != 0, (op.scanop->getNdbError()));
        op.values[i1].ra = ra;
      }
      else
      {
        NdbBlob* bh = op.scanop->getBlobHandle(i1);
        CHK2(bh != 0, (op.scanop->getNdbError()));
        op.values[i1].bh = bh;
      }
    }
    CHK1(ret == 0);

    CHK2(op.scantrans->execute(NdbTransaction::NoCommit) == 0,
         (op.scantrans->getNdbError()));
  }
  while (0);
  return ret;
}

/*
 * Copy one attribute value.  nextResult() re-defines ra/bh
 * to point to the new row.  Since we are batching, the data
 * must be saved to remain valid until execute.
 */

int
Ndb_move_data::copy_other_to_other(const Attr& attr1, const Attr& attr2)
{
  int ret = 0;
  Op& op = m_op;
  const int i1 = attr1.id;
  const int i2 = attr2.id;
  do
  {
    const NdbRecAttr* ra1 = op.values[i1].ra;
    require(ra1 != 0 && ra1->isNULL() != -1);

    if (ra1->isNULL())
    {
      const char* value = 0;
      CHK2(op.updateop->setValue(i2, value) == 0,
           (op.updateop->getNdbError()));
    }
    else
    {
      const char* value = ra1->aRef();
      CHK2(op.updateop->setValue(i2, value) == 0,
           (op.updateop->getNdbError()));
    }
  }
  while (0);
  return ret;
}

int
Ndb_move_data::copy_data_to_array(const char* data1, const Attr& attr2,
                                  Uint32 length1, Uint32 length1x)
{
  int ret = 0;
  const Opts& opts = m_opts;
  Op& op = m_op;
  const int i2 = attr2.id;
  do
  {
    if (data1 == 0)
    {
      CHK2(op.updateop->setValue(i2, data1) == 0,
           (op.updateop->getNdbError()));
    }
    else
    {
      /*
       * length1 is bytes present in data1
       * length1x may be longer for blob source
       * see invocation from array and blob source
       */
      require(length1 <= length1x);
      if (length1x > attr2.data_size)
      {
        require(opts.flags & Opts::MD_ATTRIBUTE_DEMOTION);
        length1 = attr2.data_size;
        op.truncated_in_batch++;
      }

      uchar* uptr = (uchar*)&op.buf2[0];
      switch (attr2.length_bytes) {
      case 0:
        break;
      case 1:
        require(length1 <= 0xFF);
        uptr[0] = (uchar)length1;
        break;
      case 2:
        require(length1 <= 0xFFFF);
        uptr[0] = (uchar)(length1 & 0xFF);
        uptr[1] = (uchar)(length1 >> 8);
        break;
      default:
        require(false);
        break;
      }

      char* data2 = &op.buf2[attr2.length_bytes];
      memcpy(data2, data1, length1);

      if (attr2.pad_char != -1)
      {
        char* pad_data2 = &data2[length1];
        Uint32 pad_length2 = attr2.data_size - length1;
        memset(pad_data2, attr2.pad_char, pad_length2);
      }

      CHK2(op.updateop->setValue(i2, (char*)op.buf2) == 0,
           (op.updateop->getNdbError()));
    }
  }
  while (0);
  return ret;
}

int
Ndb_move_data::copy_array_to_array(const Attr& attr1, const Attr& attr2)
{
  int ret = 0;
  Op& op = m_op;
  const int i1 = attr1.id;
  do
  {
    const NdbRecAttr* ra1 = op.values[i1].ra;
    require(ra1 != 0 && ra1->isNULL() != -1);

    if (ra1->isNULL())
    {
      const char* data1 = 0;
      CHK1(copy_data_to_array(data1, attr2, 0, 0) == 0);
    }
    else
    {
      Uint32 size1 = ra1->get_size_in_bytes();
      require(size1 >= attr1.length_bytes);
      Uint32 length1 = size1 - attr1.length_bytes;

      const char* aref1 = ra1->aRef();
      const char* data1 = &aref1[attr1.length_bytes];
      if (attr1.length_bytes == 0)
        while (length1 != 0 && data1[length1-1] == attr1.pad_char)
          length1--;
      CHK1(copy_data_to_array(data1, attr2, length1, length1) == 0);
    }
  }
  while (0);
  return ret;
}

int
Ndb_move_data::copy_array_to_blob(const Attr& attr1, const Attr& attr2)
{
  int ret = 0;
  Op& op = m_op;
  const int i1 = attr1.id;
  const int i2 = attr2.id;
  do
  {
    const NdbRecAttr* ra1 = op.values[i1].ra;
    require(ra1 != 0 && ra1->isNULL() != -1);

    NdbBlob* bh2 = op.updateop->getBlobHandle(i2);
    CHK2(bh2 != 0, (op.updateop->getNdbError()));
    if (ra1->isNULL())
    {
      CHK2(bh2->setValue(0, 0) == 0, (bh2->getNdbError()));
    }
    else
    {
      Uint32 size1 = ra1->get_size_in_bytes();
      require(size1 >= attr1.length_bytes);
      Uint32 length1 = size1 - attr1.length_bytes;

      const char* aref1 = ra1->aRef();
      const char* data1 = &aref1[attr1.length_bytes];
      if (attr1.length_bytes == 0)
        while (length1 != 0 && data1[length1-1] == attr1.pad_char)
          length1--;
      char* data1copy = alloc_data(length1);
      memcpy(data1copy, data1, length1);
      CHK2(bh2->setValue(data1copy, length1) == 0, (bh2->getNdbError()));
    }
  }
  while (0);
  return ret;
}

int
Ndb_move_data::copy_blob_to_array(const Attr& attr1, const Attr& attr2)
{
  int ret = 0;
  Op& op = m_op;
  const int i1 = attr1.id;
  do
  {
    NdbBlob* bh1 = op.values[i1].bh;
    require(bh1 != 0);

    int isNull = -1;
    CHK2(bh1->getNull(isNull) == 0, (bh1->getNdbError()));
    require(isNull == 0 || isNull == 1);
    Uint64 length64 = ~(Uint64)0;
    CHK2(bh1->getLength(length64) == 0, (bh1->getNdbError()));
    Uint32 data_length = (Uint32)length64;
    require((uint64)data_length == length64);

    if (isNull)
    {
      const char* data1 = 0;
      CHK1(copy_data_to_array(data1, attr2, 0, 0) == 0);
    }
    else
    {
      char* data1 = &op.buf1[0];
      Uint32 length1 = attr2.data_size;
      require(length1 <= (unsigned)op.buflen); // avoid buffer overflow
      CHK2(bh1->readData(data1, length1) == 0, (bh1->getNdbError()));
      // pass also real length to detect truncation
      CHK1(copy_data_to_array(data1, attr2, length1, data_length) == 0);
    }
  }
  while (0);
  return ret;
}

int
Ndb_move_data::copy_blob_to_blob(const Attr& attr1, const Attr& attr2)
{
  int ret = 0;
  Op& op = m_op;
  const int i1 = attr1.id;
  const int i2 = attr2.id;
  do
  {
    NdbBlob* bh1 = op.values[i1].bh;
    require(bh1 != 0);

    int isNull = -1;
    CHK2(bh1->getNull(isNull) == 0, (bh1->getNdbError()));
    require(isNull == 0 || isNull == 1);
    Uint64 length64 = ~(Uint64)0;
    CHK2(bh1->getLength(length64) == 0, (bh1->getNdbError()));
    Uint32 data_length = (Uint32)length64;
    require((uint64)data_length == length64);

    NdbBlob* bh2 = op.updateop->getBlobHandle(i2);
    CHK2(bh2 != 0, (op.updateop->getNdbError()));
    if (isNull)
    {
      CHK2(bh2->setValue(0, 0) == 0, (bh2->getNdbError()));
    }
    else
    {
      char* data = alloc_data(data_length);
      Uint32 bytes = data_length;

      CHK2(bh1->readData(data, bytes) == 0, (bh1->getNdbError()));
      require(bytes == data_length);
      
      // prevent TINYTEXT/TINYBLOB overflow by truncating data
      if(attr2.column->getPartSize() == 0)
      {
        Uint32 inline_size = attr2.column->getInlineSize();
        if(bytes > inline_size) 
        {
          data_length = calc_str_len_truncated(attr2.column->getCharset(),
                                               data, inline_size);
          op.truncated_in_batch++;
        }
      }
      CHK2(bh2->setValue(data, data_length) == 0,
           (bh2->getNdbError()));
    }
  }
  while (0);
  return ret;
}

int
Ndb_move_data::copy_attr(const Attr& attr1, const Attr& attr2)
{
  int ret = 0;
  require(attr1.map_id == attr2.id);
  do
  {
    if (attr1.equal && attr1.type != Attr::TypeBlob)
    {
      require(attr2.equal && attr2.type != Attr::TypeBlob);
      CHK1(copy_other_to_other(attr1, attr2) == 0);
      break;
    }
    if (attr1.type == Attr::TypeArray &&
        attr2.type == Attr::TypeArray)
    {
      CHK1(copy_array_to_array(attr1, attr2) == 0);
      break;
    }
    if (attr1.type == Attr::TypeArray &&
        attr2.type == Attr::TypeBlob)
    {
      CHK1(copy_array_to_blob(attr1, attr2) == 0);
      break;
    }
    if (attr1.type == Attr::TypeBlob &&
        attr2.type == Attr::TypeArray)
    {
      CHK1(copy_blob_to_array(attr1, attr2) == 0);
      break;
    }
    if (attr1.type == Attr::TypeBlob &&
        attr2.type == Attr::TypeBlob)
    {
      // handles TEXT and BLOB conversions 
      CHK1(copy_blob_to_blob(attr1, attr2) == 0);
      break;
    }
    require(false);
  }
  while (0);
  return ret;
}

int
Ndb_move_data::move_row()
{
  int ret = 0;
  Op& op = m_op;
  const int attrcount1 = m_source->getNoOfColumns();
  do
  {
    op.updateop = op.updatetrans->getNdbOperation(m_target);
    CHK2(op.updateop != 0, (op.updatetrans->getNdbError()));
    CHK2(op.updateop->insertTuple() == 0, (op.updateop->getNdbError()));

    for (int j = 0; j <= 1; j++)
    {
      for (int i1 = 0; i1 < attrcount1; i1++)
      {
        const Attr& attr1 = m_sourceattr[i1];
        int i2 = attr1.map_id;
        if (unlikely(i2 == -1))
          continue;
        const Attr& attr2 = m_targetattr[i2];
        const NdbDictionary::Column* c = attr2.column;
        if (j == 0 && !c->getPrimaryKey())
          continue;
        if (j == 1 && c->getPrimaryKey())
          continue;
        CHK1(copy_attr(attr1, attr2) == 0);
      }
      CHK1(ret == 0);
    }
    CHK1(ret == 0);

    CHK2(op.scanop->deleteCurrentTuple(op.updatetrans) == 0,
         (op.scanop->getNdbError()));
  }
  while (0);
  return ret;
}

int
Ndb_move_data::move_batch()
{
  int ret = 0;
  Op& op = m_op;
  op.rows_in_batch = 0;
  op.truncated_in_batch = 0;
  do
  {
    int res;
    CHK2((res = op.scanop->nextResult(true)) != -1,
         (op.scanop->getNdbError()));
    require(res == 0 || res == 1);
    if (res == 1)
    {
      op.end_of_scan = true;
      op.ndb->closeTransaction(op.scantrans);
      op.scantrans = 0;
      break;
    }

    require(op.updatetrans == 0);
    op.updatetrans = op.ndb->startTransaction();
    CHK2(op.updatetrans != 0, (op.ndb->getNdbError()));

    do
    {
      CHK1(move_row() == 0);
      op.rows_in_batch++;
      CHK2((res = op.scanop->nextResult(false)) != -1,
           (op.scanop->getNdbError()));
      require(res == 0 || res == 2);
    }
    while (res == 0);
    CHK1(ret == 0);

    if (m_error_insert && ndb_rand() % 5 == 0)
    {
      invoke_error_insert();
      CHK1(false);
    }

    CHK2(op.updatetrans->execute(NdbTransaction::Commit) == 0,
         (op.updatetrans->getNdbError()));
    op.ndb->closeTransaction(op.updatetrans);
    op.updatetrans = 0;
  }
  while (0);
  release_data();
  return ret;
}

int
Ndb_move_data::move_data(Ndb* ndb)
{
  int ret = 0;
  Op& op = m_op;
  Stat& stat = m_stat;
  stat.rows_moved = 0; // keep rows_total
  do
  {
    const NDB_TICKS now = NdbTick_getCurrentTicks(); 
    ndb_srand((unsigned)now.getUint64());
    reset_error();

    CHK2(m_source != 0 && m_target != 0,
        (Error::InvalidState, "source / target not defined"));

    op.ndb = ndb;
    CHK1(m_error.code == 0);
    CHK1(check_tables() == 0);
    CHK1(start_scan() == 0);
    while (1)
    {
      CHK1(move_batch() == 0);
      stat.rows_moved += op.rows_in_batch;
      stat.rows_total += op.rows_in_batch;
      stat.truncated += op.truncated_in_batch;

      require(op.end_of_scan == (op.rows_in_batch == 0));
      if (op.end_of_scan)
        break;
    }
    CHK1(ret == 0);
  }
  while (0);
  close_op(ndb, ret);
  return ret;
}

void
Ndb_move_data::close_op(Ndb* ndb, int ret)
{
  Op& op = m_op;
  if (ret == 0)
  {
    require(op.scantrans == 0);
    require(op.updatetrans == 0);
  }
  else
  {
    if (op.scantrans != 0)
    {
      ndb->closeTransaction(op.scantrans);
      op.scantrans = 0;
    }
    if (op.updatetrans != 0)
    {
      ndb->closeTransaction(op.updatetrans);
      op.updatetrans = 0;
    }
  }
  delete [] op.values;
  op.values = 0;
}

Ndb_move_data::Opts::Opts()
{
  flags = 0;
}

void
Ndb_move_data::set_opts_flags(int flags)
{
  Opts& opts = m_opts;
  opts.flags = flags;
}

void
Ndb_move_data::unparse_opts_tries(char* opt, const Opts::Tries& ot)
{
  sprintf(opt, "%d,%d,%d", ot.maxtries, ot.mindelay, ot.maxdelay);
}

static int
parse_opts_tries_field(const char*& t, int& out)
{
  char* u = 0;
  out = (int)strtol(t, &u, 10);
  if (t == u)
    return -1; // empty
  if (out < 0)
    return -1; // bad value
  if (*u == 0)
    return 0; // last one
  if (*u != ',')
    return -1; // bad char
  t = u + 1;
  return 1; // more after this one
}

int
Ndb_move_data::parse_opts_tries(const char* s, Opts::Tries& ot)
{
  Opts::Tries out; // a copy so nothing is set on error
  int ret;
  if (s == 0)
    return -1;
  const char* t = s;
  if ((ret = parse_opts_tries_field(t, out.maxtries)) > 0)
    if ((ret = parse_opts_tries_field(t, out.mindelay)) > 0)
      if ((ret = parse_opts_tries_field(t, out.maxdelay)) > 0)
        return -1; // too many
  if (ret < 0)
    return -1;
  if (out.mindelay > out.maxdelay)
    return -1;
  ot = out;
  return 0;
}

Ndb_move_data::Stat::Stat()
{
  rows_moved = 0;
  rows_total = 0;
  truncated = 0;
}

const Ndb_move_data::Stat&
Ndb_move_data::get_stat()
{
  return m_stat;
}

Ndb_move_data::Error::Error()
{
  line = 0;
  code = 0;
  memset(message, 0, sizeof(message));
}

bool
Ndb_move_data::Error::is_temporary() const
{
  return code > 0 && ndberror.status == NdbError::TemporaryError;
}

const Ndb_move_data::Error&
Ndb_move_data::get_error()
{
  return m_error;
}

void
Ndb_move_data::set_error_line(int line)
{
  Error& e = m_error;
  e.line = line;
}

void
Ndb_move_data::set_error_code(int code, const char* fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);
  require(code != 0);
  Error& e = m_error;
  e.code = code;
  vsnprintf(e.message, sizeof(e.message), fmt, ap);
  va_end(ap);
}

void
Ndb_move_data::set_error_code(const NdbError& ndberror)
{
  Error& e = m_error;
  set_error_code(ndberror.code, "%s", ndberror.message);
  e.ndberror = ndberror;
}

void
Ndb_move_data::reset_error()
{
  new (&m_error) Error;
}

NdbOut&
operator<<(NdbOut& out, const Ndb_move_data::Error& error)
{
  if (error.code < 0)
    out << "move data error " << error.code
        << ": " << error.message;
  else if (error.code > 0)
    out << "ndb error"
        << ": " << error.ndberror;
  else
    out << "no error";
  out << " (at lib line " << error.line << ")" << endl;
  return out;
}

void
Ndb_move_data::error_insert()
{
  m_error_insert = true;
}

void
Ndb_move_data::invoke_error_insert()
{
  NdbError ndberror;
  ndberror.code = 9999;
  ndberror.status = NdbError::TemporaryError;
  ndberror.message = "Error insert";
  set_error_line(0);
  set_error_code(ndberror);
  m_error_insert = false;
}

void
Ndb_move_data::abort_on_error()
{
  const Opts& opts = m_opts;
  require(!(opts.flags & Opts::MD_ABORT_ON_ERROR));
}
