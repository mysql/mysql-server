/*
   Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include <my_sys.h>
#include <NdbDictionaryImpl.hpp>
#include "NdbImportUtil.hpp"
// legacy
#include <BaseString.hpp>
#include <Vector.hpp>

#define snprintf BaseString::snprintf
#define vsnprintf BaseString::vsnprintf

NdbImportUtil::NdbImportUtil() :
  m_util(*this),
  c_stats(*this)
{
  c_logfile = new FileOutputStream(stderr);
  c_log = new NdbOut(*c_logfile);
  c_logmutex = NdbMutex_Create();
  require(c_logmutex != 0);
  c_logtimer.start();
  log1("ctor");
  c_rows_free = new RowList;
  c_rows_free->set_stats(m_util.c_stats, "rows-free");
  c_blobs_free = new BlobList;
  add_pseudo_tables();
}

NdbImportUtil::~NdbImportUtil()
{
  log1("dtor");
  delete c_blobs_free;
  delete c_rows_free;
  delete c_logfile;
  delete c_log;
  NdbMutex_Destroy(c_logmutex);
}

NdbOut&
operator<<(NdbOut& out, const NdbImportUtil& util)
{
  out << "util";
  return out;
}

// name

NdbImportUtil::Name::Name(const char* s)
{
  require(s != 0);
  m_str = s;
}

NdbImportUtil::Name::Name(const char* s, const char* t)
{
  require(s != 0 && t != 0);
  m_str = s;
  m_str += "-";
  m_str += t;
}

NdbImportUtil::Name::Name(const char* s, uint t)
{
  require(s != 0);
  m_str = s;
  m_str += "-";
  char b[100];
  sprintf(b, "%u", t);
  m_str += b;
}

NdbOut&
operator<<(NdbOut& out, const NdbImportUtil::Name& name)
{
  out << (const char*)name;
  return out;
}

// lockable

NdbImportUtil::Lockable::Lockable()
{
  m_mutex = NdbMutex_Create();
  m_condition = NdbCondition_Create();
  require(m_mutex != 0 && m_condition != 0);
}

NdbImportUtil::Lockable::~Lockable()
{
  NdbCondition_Destroy(m_condition);
  NdbMutex_Destroy(m_mutex);
}

void
NdbImportUtil::Lockable::lock()
{
  NdbMutex_Lock(m_mutex);
}

void
NdbImportUtil::Lockable::unlock()
{
  NdbMutex_Unlock(m_mutex);
}

void
NdbImportUtil::Lockable::wait(uint timeout)
{
  NdbCondition_WaitTimeout(m_condition, m_mutex, timeout);
}

void
NdbImportUtil::Lockable::signal()
{
  NdbCondition_Signal(m_condition);
}

// thread

NdbImportUtil::Thread::Thread()
{
  m_thread = 0;
}

NdbImportUtil::Thread::~Thread()
{
  if (m_thread != 0)
    NdbThread_Destroy(&m_thread);
}

void
NdbImportUtil::Thread::join()
{
  require(m_thread != 0);
  NdbThread_WaitFor(m_thread, (void**)0);
  NdbThread_Destroy(&m_thread);
}

// list

NdbImportUtil::ListEnt::ListEnt()
{
  m_next = 0;
  m_prev = 0;
}

NdbImportUtil::ListEnt::~ListEnt()
{
}

NdbImportUtil::List::List()
{
  m_front = 0;
  m_back = 0;
  m_cnt = 0;
  m_maxcnt = 0;
  m_totcnt = 0;
  m_stat_occup = 0;
  m_stat_total = 0;
}

NdbImportUtil::List::~List()
{
  ListEnt* ent;
  while ((ent = pop_front()) != 0)
    delete ent;
}

void
NdbImportUtil::List::set_stats(Stats& stats, const char* name)
{
  {
    const Name statname(name, "occup");
    Stat* stat = stats.create(statname, 0, 0);
    m_stat_occup = stat;
  }
  {
    const Name statname(name, "total");
    Stat* stat = stats.create(statname, 0, 0);
    m_stat_total = stat;
  }
}

void
NdbImportUtil::List::push_back(ListEnt* ent)
{
  require(ent != 0);
  require(ent->m_next == 0 && ent->m_prev == 0);
  if (m_cnt == 0)
  {
    m_front = m_back = ent;
    ent->m_next = 0;
    ent->m_prev = 0;
  }
  else
  {
    m_back->m_next = ent;
    ent->m_next = 0;
    ent->m_prev = m_back;
    m_back = ent;
  }
  m_cnt++;
  if (m_maxcnt < m_cnt)
  {
    m_maxcnt++;
    require(m_maxcnt == m_cnt);
  }
  m_totcnt++;
  validate();
  if (m_stat_occup != 0)
    m_stat_occup->add(m_cnt);
  if (m_stat_total != 0)
    m_stat_total->add(1);
}

void
NdbImportUtil::List::push_front(ListEnt* ent)
{
  require(ent != 0);
  require(ent->m_next == 0 && ent->m_prev == 0);
  if (m_cnt == 0)
  {
    m_front = m_back = ent;
    ent->m_next = 0;
    ent->m_prev = 0;
  }
  else
  {
    m_front->m_prev = ent;
    ent->m_prev = 0;
    ent->m_next = m_front;
    m_front = ent;
  }
  m_cnt++;
  if (m_maxcnt < m_cnt)
  {
    m_maxcnt++;
    require(m_maxcnt == m_cnt);
  }
  m_totcnt++;
  validate();
  if (m_stat_occup != 0)
    m_stat_occup->add(m_cnt);
  if (m_stat_total != 0)
    m_stat_total->add(1);
}

NdbImportUtil::ListEnt*
NdbImportUtil::List::pop_front()
{
  ListEnt* ent = 0;
  if (m_cnt != 0)
  {
    if (m_cnt == 1)
    {
      ent = m_front;
      m_front = m_back = 0;
    }
    else
    {
      ent = m_front;
      m_front = ent->m_next;
      m_front->m_prev = 0;
      ent->m_next = 0;
      ent->m_prev = 0;
    }
    m_cnt--;
    validate();
    if (m_stat_occup != 0)
      m_stat_occup->add(m_cnt);
  }
  return ent;
}

void
NdbImportUtil::List::remove(ListEnt* ent)
{
  ListEnt* prev = ent->m_prev;
  ListEnt* next = ent->m_next;
  ent->m_prev = 0;
  ent->m_next = 0;
  if (prev != 0)
    prev->m_next = next;
  if (next != 0)
    next->m_prev = prev;
  if (m_front == ent)
    m_front = next;
  if (m_back == ent)
    m_back = prev;
  require(m_cnt != 0);
  m_cnt--;
  validate();
  if (m_stat_occup != 0)
    m_stat_occup->add(m_cnt);
}

void
NdbImportUtil::List::push_back(List& list2)
{
  if (list2.m_cnt != 0)
  {
    if (m_cnt != 0)
    {
      ListEnt* ent1 = m_back;
      ListEnt* ent2 = list2.m_front;
      require(ent1 != 0 && ent2 != 0);
      require(ent1->m_next == 0 && ent2->m_prev == 0);
      // push list2 to the back
      ent1->m_next = ent2;
      ent2->m_prev = ent1;
      m_back = list2.m_back;
      m_cnt += list2.m_cnt;
    }
    else
    {
      m_front = list2.m_front;
      m_back = list2.m_back;
      m_cnt = list2.m_cnt;
    }
    if (m_maxcnt < m_cnt)
      m_maxcnt = m_cnt;
    m_totcnt += list2.m_cnt;
  }
  validate();
  // erase list2 but leave stats alone
  list2.m_front = 0;
  list2.m_back = 0;
  list2.m_cnt = 0;
}

#if defined(VM_TRACE) || defined(TEST_NDBIMPORTUTIL)
void
NdbImportUtil::List::validate() const
{
  if (m_cnt == 0)
  {
    require(m_front == 0);
    require(m_back == 0);
  }
  else
  {
    require(m_front != 0);
    require(m_front->m_prev == 0);
    require(m_back != 0);
    require(m_back->m_next == 0);
    if (m_cnt == 1)
      require(m_front == m_back);
    else
      require(m_front != m_back);
  }
#if defined(TEST_NDBIMPORTUTIL)
  uint cnt = 0;
  ListEnt* ent1 = m_front;
  ListEnt* ent2 = 0;
  while (ent1 != 0)
  {
    require(ent1->m_prev == ent2);
    if (ent2 != 0)
      require(ent2->m_next == ent1);
    ent2 = ent1;
    ent1 = ent1->m_next;
    cnt++;
  }
  require(m_cnt == cnt);
#endif
}
#endif

// attrs

NdbImportUtil::Attr::Attr()
{
  m_attrname = "";
  m_attrno = Inval_uint;
  m_attrid = Inval_uint;
  m_type = NdbDictionary::Column::Undefined;
  m_charset = 0;
  memset(m_sqltype, 0, sizeof(m_sqltype));
  m_pk = false;
  m_nullable = false;
  m_precision = 0;
  m_scale = 0;
  m_length = 0;
  m_charlength = 0;
  m_arraytype = NdbDictionary::Column::ArrayTypeFixed;
  m_inlinesize = 0;
  m_partsize = 0;
  m_blobtable = 0;
  m_size = 0;
  m_pad = false;
  m_padchar = 0;
  m_quotable = false;
  m_isblob = false;
  m_blobno = Inval_uint;
  m_offset = 0;
  m_null_byte = Inval_uint;
  m_null_bit = Inval_uint;
}

void
NdbImportUtil::Attr::set_value(Row* row, const void* data, uint len) const
{
  require(!m_isblob);
  require(data != 0);
  uint totlen = m_arraytype + len;
  require(totlen <= m_size);
  require(m_offset + totlen <= row->m_rowsize);
  uchar* p = &row->m_data[m_offset];
  switch (m_arraytype) {
  case 0:
    break;
  case 1:
    require(len <= 0xff);
    p[0] = (uchar)len;
    p += 1;
    break;
  case 2:
    require(len <= 0xffff);
    p[0] = (uchar)(len & 0xff);
    p[1] = (uchar)(len >> 8);
    p += 2;
    break;
  default:
    require(false);
    break;
  }
  memcpy(p, data, len);
  if (m_pad)
    memset(&p[totlen], m_padchar, m_size - totlen);
  if (m_nullable)
    set_null(row, false);
}

void
NdbImportUtil::Attr::set_blob(Row* row, const void* data, uint len) const
{
  require(m_isblob);
  require(data != 0);
  require(m_blobno < row->m_blobs.size());
  Blob* blob = row->m_blobs[m_blobno];
  require(blob != 0);
  blob->resize(len);
  memcpy(blob->m_data, data, len);
  blob->m_blobsize = len;
  if (m_nullable)
    set_null(row, false);
}

void
NdbImportUtil::Attr::set_null(Row* row, bool null) const
{
  uchar* data = row->m_data;
  uchar mask = (1 << m_null_bit);
  if (null)
    data[m_null_byte] |= mask;
  else
    data[m_null_byte] &= ~mask;
}

const void*
NdbImportUtil::Attr::get_value(const Row* row) const
{
  const uchar* p = &row->m_data[m_offset];
  return p;
}

bool
NdbImportUtil::Attr::get_null(const Row* row) const
{
  bool null = false;
  if (m_nullable)
  {
    const uchar* data = row->m_data;
    uchar mask = (1 << m_null_bit);
    null = (bool)(data[m_null_byte] & mask);
  }
  return null;
}

uint
NdbImportUtil::Attr::get_blob_parts(uint len) const
{
  require(m_isblob);
  uint parts = 0;
  if (len > m_inlinesize)
  {
    require(m_partsize != 0);
    parts = (len -m_inlinesize + m_partsize - 1) / m_partsize;
  }
  return parts;
}

// could go in NdbDictionary
void
NdbImportUtil::Attr::set_sqltype()
{
  m_sqltype[0] = 0;
  switch (m_type) {
  case NdbDictionary::Column::Tinyint:
    sprintf(m_sqltype, "tinyint");
    break;
  case NdbDictionary::Column::Smallint:
    sprintf(m_sqltype, "smallint");
    break;
  case NdbDictionary::Column::Mediumint:
    sprintf(m_sqltype, "mediumint");
    break;
  case NdbDictionary::Column::Int:
    sprintf(m_sqltype, "int");
    break;
  case NdbDictionary::Column::Bigint:
    sprintf(m_sqltype, "bigint");
    break;
  case NdbDictionary::Column::Tinyunsigned:
    sprintf(m_sqltype, "tinyint unsigned");
    break;
  case NdbDictionary::Column::Smallunsigned:
    sprintf(m_sqltype, "smallint unsigned");
    break;
  case NdbDictionary::Column::Mediumunsigned:
    sprintf(m_sqltype, "mediumint unsigned");
    break;
  case NdbDictionary::Column::Unsigned:
    sprintf(m_sqltype, "int unsigned");
    break;
  case NdbDictionary::Column::Bigunsigned:
    sprintf(m_sqltype, "bigint unsigned");
    break;
  case NdbDictionary::Column::Decimal:
    sprintf(m_sqltype, "decimal");
    break;
  case NdbDictionary::Column::Decimalunsigned:
    sprintf(m_sqltype, "decimal unsigned");
    break;
  case NdbDictionary::Column::Float:
    sprintf(m_sqltype, "float");
    break;
  case NdbDictionary::Column::Double:
    sprintf(m_sqltype, "double");
    break;
  case NdbDictionary::Column::Char:
    require(m_charset != 0 && m_charset->csname != 0);
    sprintf(m_sqltype, "char(%u) %s", m_charlength, m_charset->csname);
    break;
  case NdbDictionary::Column::Varchar:
    require(m_charset != 0 && m_charset->csname != 0);
    sprintf(m_sqltype, "varchar(%u) %s", m_charlength, m_charset->csname);
    break;
  case NdbDictionary::Column::Longvarchar:
    require(m_charset != 0 && m_charset->csname != 0);
    sprintf(m_sqltype, "varchar(%u) %s", m_charlength, m_charset->csname);
    break;
  case NdbDictionary::Column::Binary:
    sprintf(m_sqltype, "binary(%u)", m_length);
    break;
  case NdbDictionary::Column::Varbinary:
    sprintf(m_sqltype, "varbinary(%u)", m_length);
    break;
  case NdbDictionary::Column::Longvarbinary:
    sprintf(m_sqltype, "varbinary(%u)", m_length);
    break;
  case NdbDictionary::Column::Bit:
    sprintf(m_sqltype, "bit(%u)", m_length);
    break;
  case NdbDictionary::Column::Year:
    sprintf(m_sqltype, "year");
    break;
  case NdbDictionary::Column::Date:
    sprintf(m_sqltype, "date");
    break;
  case NdbDictionary::Column::Time2:
    if (m_precision == 0)
      sprintf(m_sqltype, "time");
    else
      sprintf(m_sqltype, "time(%u)", m_precision);
    break;
  case NdbDictionary::Column::Datetime2:
    if (m_precision == 0)
      sprintf(m_sqltype, "datetime");
    else
      sprintf(m_sqltype, "datetime(%u)", m_precision);
    break;
  case NdbDictionary::Column::Timestamp2:
    if (m_precision == 0)
      sprintf(m_sqltype, "timestamp");
    else
      sprintf(m_sqltype, "timestamp(%u)", m_precision);
    break;
  case NdbDictionary::Column::Blob:
    sprintf(m_sqltype, "blob");
    break;
  case NdbDictionary::Column::Text:
    sprintf(m_sqltype, "text");
    break;
  default:
    sprintf(m_sqltype, "unknown type=%d", (int)m_type);
    break;
  }
}

// tables

NdbImportUtil::Table::Table()
{
  m_tabid = Inval_uint;
  m_tab = 0;
  m_rec = 0;
  m_keyrec = 0;
  m_rowsize = 0;
  m_has_hidden_pk = false;
}

void
NdbImportUtil::Table::add_pseudo_attr(const char* name,
                                      NdbDictionary::Column::Type type,
                                      uint length)
{
  const uint id = m_attrs.size();
  Attr attr;    // ctor sets defaults
  attr.m_attrname = name;
  attr.m_attrno = id;
  attr.m_attrid = id;
  attr.m_type = type;
  attr.m_length = length;
  attr.m_charlength = length;
  switch (type) {
  case NdbDictionary::Column::Unsigned:
    require(length == 1);
    attr.m_arraytype = NdbDictionary::Column::ArrayTypeFixed;
    attr.m_size = 4;
    attr.m_quotable = false;
    break;
  case NdbDictionary::Column::Bigunsigned:
    require(length == 1);
    attr.m_arraytype = NdbDictionary::Column::ArrayTypeFixed;
    attr.m_size = 8;
    attr.m_quotable = false;
    break;
  case NdbDictionary::Column::Double:
    require(length == 1);
    attr.m_arraytype = NdbDictionary::Column::ArrayTypeFixed;
    attr.m_size = 8;
    attr.m_quotable = false;
    break;
  case NdbDictionary::Column::Varchar:
    require(length > 1);
    attr.m_charset = &my_charset_bin;
    attr.m_arraytype = NdbDictionary::Column::ArrayTypeShortVar;
    attr.m_size = 1 + length;
    attr.m_quotable = true;
    break;
  case NdbDictionary::Column::Longvarchar:
    require(length > 1);
    attr.m_charset = &my_charset_bin;
    attr.m_arraytype = NdbDictionary::Column::ArrayTypeMediumVar;
    attr.m_size = 2 + length;
    attr.m_quotable = true;
    break;
  case NdbDictionary::Column::Text:
    attr.m_charset = &my_charset_bin;
    attr.m_inlinesize = 256;
    attr.m_partsize = 2000;
    attr.m_isblob = true;
    attr.m_blobno = m_blobids.size();
    m_blobids.push_back(id);
    attr.m_quotable = true;
    break;
  default:
    require(false);
    break;
  };
  attr.set_sqltype();
  if (id == 0)
    attr.m_offset = 0;
  else
  {
    const Attr& prevattr = m_attrs[id - 1];
    attr.m_offset = prevattr.m_offset + prevattr.m_size;
  }
  attr.m_null_byte = Inval_uint;
  attr.m_null_bit = Inval_uint;
  m_rowsize += attr.m_size;
  m_attrs.push_back(attr);
}

const NdbImportUtil::Attr&
NdbImportUtil::Table::get_attr(const char* attrname) const
{
  uint i = 0;
  const uint n = m_attrs.size();
  while (i < n)
  {
    if (strcmp(m_attrs[i].m_attrname.c_str(), attrname) == 0)
      break;
    i++;
  }
  require(i < n);
  return m_attrs[i];
}

uint
NdbImportUtil::Table::get_nodeid(uint fragid) const
{
  require(fragid < m_fragments.size());
  uint nodeid = m_fragments[fragid];
  return nodeid;
}

int
NdbImportUtil::add_table(NdbDictionary::Dictionary* dic,
                         const NdbDictionary::Table* tab,
                         uint& tabid,
                         Error& error)
{
  require(tab != 0);
  require(tab->getObjectStatus() == NdbDictionary::Object::Retrieved);
  log1("add_table: " << tab->getName());
  tabid = tab->getObjectId();
  // check if mapped already
  {
    std::map<uint, Table>::const_iterator it;
    it = c_tables.m_tables.find(tabid);
    if (it != c_tables.m_tables.end())
    {
      const Table& table = it->second;
      require(table.m_tab == tab);
      return 0;
    }
  }
  Table table;
  do
  {
    const NdbRecord* rec = tab->getDefaultRecord();
    table.m_tabid = tabid;
    table.m_tab = tab;
    table.m_rec = rec;
    table.m_rowsize = NdbDictionary::getRecordRowLength(rec);
    Attrs& attrs = table.m_attrs;
    const uint attrcnt = tab->getNoOfColumns();
    attrs.reserve(attrcnt);
    bool ok = true;
    Uint32 recAttrId;
    for (uint i = 0; i < attrcnt && ok; i++)
    {
      if (i == 0)
        require(NdbDictionary::getFirstAttrId(rec, recAttrId));
      else
        require(NdbDictionary::getNextAttrId(rec, recAttrId));
      require(recAttrId == i);
      Attr attr;
      const NdbDictionary::Column* col = tab->getColumn(i);
      require(col !=0);
      attr.m_attrname = col->getName();
      attr.m_attrno = i;
      attr.m_attrid = i;
      attr.m_type = col->getType();
      attr.m_pk = col->getPrimaryKey();
      attr.m_nullable = col->getNullable();
      attr.m_precision = col->getPrecision();
      attr.m_scale = col->getScale();
      attr.m_length = col->getLength();
      attr.m_arraytype = col->getArrayType();
      require(attr.m_arraytype <= 2);
      attr.m_size = col->getSizeInBytes();
      switch (attr.m_type) {
      case NdbDictionary::Column::Char:
        attr.m_pad = true;
        attr.m_padchar = 0x20;
        break;
      case NdbDictionary::Column::Binary:
        attr.m_pad = true;
        attr.m_padchar = 0x0;
        break;
      default:
        attr.m_pad = false;
        break;
      }
      switch (attr.m_type) {
      case NdbDictionary::Column::Char:
      case NdbDictionary::Column::Varchar:
      case NdbDictionary::Column::Longvarchar:
        attr.m_charset = col->getCharset();
        require(attr.m_charset != 0);
        uint mbmaxlen; mbmaxlen = attr.m_charset->mbmaxlen;
        require(mbmaxlen != 0);
        require(attr.m_length % mbmaxlen == 0);
        attr.m_charlength = attr.m_length / mbmaxlen;
        attr.m_quotable = true;
        break;
      case NdbDictionary::Column::Text:
        attr.m_charset = col->getCharset();
        require(attr.m_charset != 0);
        break;
      case NdbDictionary::Column::Binary:
      case NdbDictionary::Column::Varbinary:
      case NdbDictionary::Column::Longvarbinary:
        attr.m_charset = 0;
        attr.m_charlength = attr.m_length;
        attr.m_quotable = true;
        break;
      default:
        attr.m_charset = 0;
        attr.m_charlength = attr.m_length;
        attr.m_quotable = false;
        break;
      }
      switch (attr.m_type) {
      case NdbDictionary::Column::Blob:
      case NdbDictionary::Column::Text:
        attr.m_isblob = true;
        attr.m_inlinesize = col->getInlineSize();
        attr.m_partsize = col->getPartSize();
        attr.m_blobno = table.m_blobids.size();
        attr.m_blobtable = col->getBlobTable();
        if (attr.m_partsize == 0)
          require(attr.m_blobtable == 0);
        else
          require(attr.m_blobtable != 0);
        table.m_blobids.push_back(i);
        break;
      default:
        attr.m_isblob = false;
        break;
      }
      attr.set_sqltype();
      Uint32 offset;
      require(NdbDictionary::getOffset(rec, i, offset));
      attr.m_offset = offset;
      Uint32 null_byte, null_bit;
      require(NdbDictionary::getNullBitOffset(rec, i, null_byte, null_bit));
      attr.m_null_byte = null_byte;
      attr.m_null_bit = null_bit;
      attrs.push_back(attr);
    }
    if (!ok)
      break;
    require(!NdbDictionary::getNextAttrId(rec, recAttrId));
    NdbDictionary::RecordSpecification
      speclist[NDB_MAX_NO_OF_ATTRIBUTES_IN_KEY];
    uint nkey = 0;
    for (uint i = 0; i < attrcnt && ok; i++)
    {
      const Attr& attr = attrs[i];
      if (attr.m_pk)
      {
        const NdbDictionary::Column* col = tab->getColumn(i);
        require(col->getPrimaryKey());
        NdbDictionary::RecordSpecification& spec = speclist[nkey];
        spec.column = col;
        spec.offset = attr.m_offset;
        spec.nullbit_byte_offset = attr.m_null_byte;
        spec.nullbit_bit_in_byte = attr.m_null_bit;
        nkey++;
        // guess hidden pk
        if (strcmp(attr.m_attrname.c_str(), "$PK") == 0)
        {
          if (i + 1 == attrcnt &&
              nkey == 1 &&
              attr.m_type == NdbDictionary::Column::Bigunsigned)
            table.m_has_hidden_pk = true;
          else
          {
            set_error_usage(error, __LINE__,
                            "column %u: "
                            "invalid use of reserved column name $PK", i);
            ok = false;
            break;
          }
        }
      }
    }
    if (!ok)
      break;
    require(nkey == (uint)tab->getNoOfPrimaryKeys());
    const NdbRecord* keyrec =
      dic->createRecord(tab,
                        speclist,
                        nkey,
                        sizeof(NdbDictionary::RecordSpecification));
    if (keyrec == 0)
    {
      m_util.set_error_ndb(error, __LINE__, dic->getNdbError());
      break;
    }
    table.m_keyrec = keyrec;
    {
      NdbTableImpl& tabImpl = NdbTableImpl::getImpl(*tab);
      const Vector<Uint16>& fragments = tabImpl.m_fragments;
      for (uint i = 0; i < fragments.size(); i++)
      {
        uint16 nodeid = fragments[i];
        table.m_fragments.push_back(nodeid);
      }
    }
    c_tables.m_tables.insert(std::pair<uint, Table>(tabid, table));
    return 0;
  } while (0);
  return -1;
}

const NdbImportUtil::Table&
NdbImportUtil::get_table(uint tabid)
{
  std::map<uint, Table>::const_iterator it;
  it = c_tables.m_tables.find(tabid);
  require(it != c_tables.m_tables.end());
  const Table& table = it->second;
  return table;
}

// rows

NdbImportUtil::Row::Row()
{
  m_tabid = Inval_uint;
  m_rowsize = 0;
  m_allocsize = 0;
  m_rowid = Inval_uint64;
  m_linenr = Inval_uint64;
  m_startpos = Inval_uint64;
  m_endpos = Inval_uint64;
  m_data = 0;
}

NdbImportUtil::Row::~Row()
{
  delete [] m_data;
}

void
NdbImportUtil::Row::init(const Table& table)
{
  m_tabid = table.m_tabid;
  uint rowsize = table.m_rowsize;
  require(rowsize > 0);
  m_rowsize = rowsize;
  if (m_allocsize < rowsize)
  {
    delete [] m_data;
    m_data = new uchar [rowsize];
    m_allocsize = rowsize;
  }
}

NdbImportUtil::RowList::RowList()
{
  m_rowsize = 0;
  m_rowbatch = UINT_MAX;
  m_rowbytes = UINT_MAX;
  m_eof = false;
  m_foe = false;
  m_overflow = 0;
  m_underflow = 0;
  m_stat_overflow = 0;
  m_stat_underflow = 0;
}

NdbImportUtil::RowList::~RowList ()
{
}

void
NdbImportUtil::RowList::set_stats(Stats& stats, const char* name)
{
  List::set_stats(stats, name);
  {
    const Name statname(name, "overflow");
    Stat* stat = stats.create(statname, 0, 0);
    m_stat_overflow = stat;
  }
  {
    const Name statname(name, "underflow");
    Stat* stat = stats.create(statname, 0, 0);
    m_stat_underflow = stat;
  }
}

bool
NdbImportUtil::RowList::push_back(Row* row)
{
  bool ret = false;
  if (m_cnt < m_rowbatch && m_rowsize < m_rowbytes)
  {
    List::push_back(row);
    m_rowsize += row->m_rowsize;
    ret = true;
  }
  else
  {
    m_overflow++;
    if (m_stat_overflow != 0)
      m_stat_overflow->add(1);
  }
  return ret;
}

void
NdbImportUtil::RowList::push_back_force(Row* row)
{
  List::push_back(row);
  m_rowsize += row->m_rowsize;
}

bool
NdbImportUtil::RowList::push_front(Row* row)
{
  bool ret = false;
  if (m_cnt < m_rowbatch && m_rowsize < m_rowbytes)
  {
    List::push_front(row);
    m_rowsize += row->m_rowsize;
    ret = true;
  }
  else
  {
    m_overflow++;
    if (m_stat_overflow != 0)
      m_stat_overflow->add(1);
  }
  return ret;
}

NdbImportUtil::Row*
NdbImportUtil::RowList::pop_front()
{
  Row* row = 0;
  do
  {
    row = static_cast<Row*>(List::pop_front());
    if (row != 0)
    {
      require(m_rowsize >= row->m_rowsize);
      m_rowsize -= row->m_rowsize;
      break;
    }
    m_underflow++;
    if (m_stat_underflow != 0)
      m_stat_underflow->add(1);
  } while (0);
  return row;
}

void
NdbImportUtil::RowList::remove(Row* row)
{
  List::remove(row);
  require(m_rowsize >= row->m_rowsize);
  m_rowsize -= row->m_rowsize;
}

NdbImportUtil::Row*
NdbImportUtil::alloc_row(const Table& table)
{
  RowList& rows = *c_rows_free;
  rows.lock();
  Row* row = rows.pop_front();
  rows.unlock();
  if (row == 0)
  {
    row = new Row;
  }
  row->init(table);
  while (row->m_blobs.size() < table.m_blobids.size())
  {
    Blob* blob = alloc_blob();
    row->m_blobs.push_back(blob);
  }
  return row;
}

void
NdbImportUtil::free_row(Row* row)
{
  RowList& rows = *c_rows_free;
  rows.lock();
  rows.push_back(row);
  rows.unlock();
}

// blobs

NdbImportUtil::Blob::Blob()
{
  m_blobsize = 0;
  m_allocsize = 0;
  m_data = new uchar [0];
}

NdbImportUtil::Blob::~Blob()
{
  delete [] m_data;
}

NdbImportUtil::BlobList::BlobList()
{
}

NdbImportUtil::BlobList::~BlobList()
{
}

void
NdbImportUtil::Blob::resize(uint size)
{
  if (m_allocsize < size)
  {
    delete [] m_data;
    m_data = new uchar [size];
    m_allocsize = size;
  }
}

NdbImportUtil::Blob*
NdbImportUtil::alloc_blob()
{
  BlobList& blobs = *c_blobs_free;
  blobs.lock();
  Blob* blob = static_cast<Blob*>(blobs.pop_front());
  blobs.unlock();
  if (blob == 0)
  {
    blob = new Blob;
  }
  return blob;
}

void
NdbImportUtil::free_blob(Blob* blob)
{
  BlobList& blobs = *c_blobs_free;
  blobs.lock();
  blobs.push_back(blob);
  blobs.unlock();
}

// rowmap

void
NdbImportUtil::RowMap::add(Range r)
{
  require(r.m_start < r.m_end);
  if (unlikely(m_ranges.empty()))
  {
    m_ranges.push_back(r);
  }
  else
  {
    Iterator itbegin = m_ranges.begin();
    Iterator itend = m_ranges.end();
    Range& rback = m_ranges.back();
    Range& rfront = m_ranges.front();
    if (rback.m_start < r.m_start)
    {
      if (merge_down(rback, r))
      {
        // rback grows up to include r
        ;
      }
      else
      {
        m_ranges.push_back(r);
      }
    }
    else if (rfront.m_start > r.m_start)
    {
      if (merge_up(rfront, r))
      {
        // rfront grows down to include r
        ;
      }
      else
      {
        m_ranges.insert(itbegin, r);
      }
    }
    else
    {
      Iterator it = std::lower_bound(itbegin, itend, r);
      require(it > itbegin);
      require(it < itend);
      Range& rprev = *(it - 1);
      Range& rnext = *it;
      if (merge_down(rprev, r))
      {
        // rprev grows up to include r
        if (merge_down(rprev, rnext))
        {
          // rprev grows up to include rnext
          // erase rnext
          m_ranges.erase(it);
        }
      }
      else if (merge_up(rnext, r))
      {
        // rnext grows down to include r
        ;
      }
      else
      {
        m_ranges.insert(it, r);
      }
    }
  }
}

void
NdbImportUtil::RowMap::add(const RowMap& m)
{
  const Ranges& ranges = m.m_ranges;
  ConstIterator it;
  for (it = ranges.begin(); it < ranges.end(); it++)
  {
    Range r = *it;
    add(r);
  }
}

bool
NdbImportUtil::RowMap::find(uint64 rowid, Iterator& itout)
{
  if (unlikely(m_ranges.empty()))
    return false;
  Range r;
  r.m_start = rowid;
  r.m_end = rowid + 1;
  r.m_reject = 0;
  Iterator itbegin = m_ranges.begin();
  Iterator itend = m_ranges.end();
  Iterator it = std::lower_bound(itbegin, itend, r);
  if (it == itbegin)
  {
    Range& rfront = *it;
    if (r.m_start < rfront.m_start)
      return false;
    require(r.m_start == rfront.m_start);
    itout = it;
    return true;
  }
  if (it == itend)
  {
    Range& rback = *(it - 1);
    require(r.m_start > rback.m_start);
    if (r.m_end <= rback.m_end)
    {
      itout = it - 1;
      return true;
    }
    return false;
  }
  {
    Range& rcurr = *it;
    require(r.m_start <= rcurr.m_start);
    if (r.m_start == rcurr.m_start)
    {
      itout = it;
      return true;
    }
  }
  {
    Range& rprev = *(it - 1);
    require(r.m_start > rprev.m_start);
    if (r.m_end <= rprev.m_end)
    {
      itout = it - 1;
      return true;
    }
    return false;
  }
}

bool
NdbImportUtil::RowMap::remove(uint64 rowid)
{
  Iterator it;
  if (!find(rowid, it))
    return false;
  Range& r = *it;
  require(rowid >= r.m_start);
  require(rowid < r.m_end);
  if (rowid == r.m_start)
  {
    r.m_start += 1;
    if (r.m_start == r.m_end)
      m_ranges.erase(it);
  }
  else if (rowid == r.m_end - 1)
  {
    r.m_end -= 1;
    require(r.m_start < r.m_end);
  }
  else
  {
    Range r2;
    r2.m_start = rowid + 1;
    r2.m_end = r.m_end;
    r2.m_reject = 0;
    require(r2.m_start < r2.m_end);
    r.m_end = rowid;
    require(r.m_start < r.m_end);
    m_ranges.insert(it + 1, r2);
  }
  return true;
}

void
NdbImportUtil::RowMap::get_total(uint64& rows, uint64& reject) const
{
  uint64 t = 0;
  uint64 r = 0;
  ConstIterator it;
  for (it = m_ranges.begin(); it < m_ranges.end(); it++)
  {
    t += it->m_end - it->m_start - it->m_reject;
    r += it->m_reject;
  }
  rows = t;
  reject = r;
}

NdbOut&
operator<<(NdbOut& out, const NdbImportUtil::RowMap::Range& range)
{
  out << "start=" << range.m_start
      << " end=" << range.m_end
      << " rows=" << range.m_end - range.m_start
      << " startpos=" << range.m_startpos
      << " endpos=" << range.m_endpos
      << " bytes=" << range.m_endpos - range.m_startpos
      << " reject=" << range.m_reject;
  return out;
}

NdbOut&
operator<<(NdbOut& out, const NdbImportUtil::RowMap& rowmap)
{
  const NdbImportUtil::RowMap::Ranges& ranges = rowmap.m_ranges;
  NdbImportUtil::RowMap::ConstIterator it;
  uint i = 0;
  for (it = ranges.begin(); it < ranges.end(); it++)
  {
    out << endl;
    const NdbImportUtil::RowMap::Range& range = *it;
    out << i << ": " << range;
  }
  return out;
}

// pseudo-tables

void
NdbImportUtil::add_pseudo_tables()
{
  add_result_table();
  add_reject_table();
  add_rowmap_table();
  add_stats_table();
}

void
NdbImportUtil::add_result_table()
{
  Table& table = c_result_table;
  table.m_tabid = g_result_tabid;
  require(table.m_rowsize == 0);
  table.add_pseudo_attr("runno",
                        NdbDictionary::Column::Unsigned);
  table.add_pseudo_attr("name",
                        NdbDictionary::Column::Varchar,
                        10);
  table.add_pseudo_attr("desc",
                        NdbDictionary::Column::Varchar,
                        100);
  table.add_pseudo_attr("result",
                        NdbDictionary::Column::Unsigned);
  table.add_pseudo_attr("rows",
                        NdbDictionary::Column::Bigunsigned);
  table.add_pseudo_attr("reject",
                        NdbDictionary::Column::Bigunsigned);
  table.add_pseudo_attr("temperrors",
                        NdbDictionary::Column::Bigunsigned);
  table.add_pseudo_attr("runtime",
                        NdbDictionary::Column::Bigunsigned);
  table.add_pseudo_attr("utime",
                        NdbDictionary::Column::Bigunsigned);
  add_error_attrs(table);
}

void
NdbImportUtil::add_reject_table()
{
  Table& table = c_reject_table;
  table.m_tabid = g_reject_tabid;
  require(table.m_rowsize == 0);
  table.add_pseudo_attr("runno",
                        NdbDictionary::Column::Unsigned);
  table.add_pseudo_attr("rowid",
                        NdbDictionary::Column::Bigunsigned);
  table.add_pseudo_attr("linenr",
                        NdbDictionary::Column::Bigunsigned);
  table.add_pseudo_attr("startpos",
                        NdbDictionary::Column::Bigunsigned);
  table.add_pseudo_attr("endpos",
                        NdbDictionary::Column::Bigunsigned);
  table.add_pseudo_attr("bytes",
                        NdbDictionary::Column::Bigunsigned);
  add_error_attrs(table);
  table.add_pseudo_attr("reject",
                        NdbDictionary::Column::Text);
}
 
void
NdbImportUtil::add_rowmap_table()
{
  Table& table = c_rowmap_table;
  table.m_tabid = g_rowmap_tabid;
  require(table.m_rowsize == 0);
  table.add_pseudo_attr("runno",
                        NdbDictionary::Column::Unsigned);
  table.add_pseudo_attr("start",
                        NdbDictionary::Column::Bigunsigned);
  table.add_pseudo_attr("end",
                        NdbDictionary::Column::Bigunsigned);
  table.add_pseudo_attr("rows",
                        NdbDictionary::Column::Bigunsigned);
  table.add_pseudo_attr("startpos",
                        NdbDictionary::Column::Bigunsigned);
  table.add_pseudo_attr("endpos",
                        NdbDictionary::Column::Bigunsigned);
  table.add_pseudo_attr("bytes",
                        NdbDictionary::Column::Bigunsigned);
  table.add_pseudo_attr("reject",
                        NdbDictionary::Column::Bigunsigned);
}

void
NdbImportUtil::set_rowmap_row(Row* row,
                              uint32 runno,
                              const RowMap::Range& range)
{
  const Table& table = c_rowmap_table;
  const Attrs& attrs = table.m_attrs;
  uint id = 0;
  // runno
  {
    const Attr& attr = attrs[id];
    attr.set_value(row, &runno, sizeof(runno));
    id++;
  }
  // start
  {
    const Attr& attr = attrs[id];
    attr.set_value(row, &range.m_start, sizeof(range.m_start));
    id++;
  }
  // end
  {
    const Attr& attr = attrs[id];
    attr.set_value(row, &range.m_end, sizeof(range.m_end));
    id++;
  }
  // rows
  {
    const Attr& attr = attrs[id];
    uint64 rows = range.m_end - range.m_start;
    attr.set_value(row, &rows, sizeof(rows));
    id++;
  }
  // startpos
  {
    const Attr& attr = attrs[id];
    attr.set_value(row, &range.m_startpos, sizeof(range.m_startpos));
    id++;
  }
  // end
  {
    const Attr& attr = attrs[id];
    attr.set_value(row, &range.m_endpos, sizeof(range.m_endpos));
    id++;
  }
  // bytes
  {
    const Attr& attr = attrs[id];
    uint64 bytes = range.m_endpos - range.m_startpos;
    attr.set_value(row, &bytes, sizeof(bytes));
    id++;
  }
  // reject
  {
    const Attr& attr = attrs[id];
    attr.set_value(row, &range.m_reject, sizeof(range.m_reject));
    id++;
  }
  require(id == attrs.size());
}

void
NdbImportUtil::add_stats_table()
{
  Table& table = c_stats_table;
  table.m_tabid = g_stats_tabid;
  require(table.m_rowsize == 0);
  table.add_pseudo_attr("runno",
                        NdbDictionary::Column::Unsigned);
  table.add_pseudo_attr("id",
                        NdbDictionary::Column::Unsigned);
  table.add_pseudo_attr("name",
                        NdbDictionary::Column::Varchar,
                        100);
  table.add_pseudo_attr("parent",
                        NdbDictionary::Column::Unsigned);
  table.add_pseudo_attr("obs",
                        NdbDictionary::Column::Bigunsigned);
  table.add_pseudo_attr("sum",
                        NdbDictionary::Column::Bigunsigned);
  table.add_pseudo_attr("mean",
                        NdbDictionary::Column::Double);
  table.add_pseudo_attr("min",
                        NdbDictionary::Column::Bigunsigned);
  table.add_pseudo_attr("max",
                        NdbDictionary::Column::Bigunsigned);
  table.add_pseudo_attr("stddev",
                        NdbDictionary::Column::Double);
}

void
NdbImportUtil::add_error_attrs(Table& table)
{
  table.add_pseudo_attr("errortype",
                        NdbDictionary::Column::Varchar,
                        10);
  table.add_pseudo_attr("errorcode",
                        NdbDictionary::Column::Unsigned);
  table.add_pseudo_attr("sourceline",
                        NdbDictionary::Column::Unsigned);
  table.add_pseudo_attr("errortext",
                        NdbDictionary::Column::Longvarchar,
                        1024);
}

void
NdbImportUtil::set_result_row(Row* row,
                              uint32 runno,
                              const char* name,
                              const char* desc,
                              uint64 rows,
                              uint64 reject,
                              uint64 temperrors,
                              uint64 runtime,
                              uint64 utime,
                              const Error& error)
{
  const Table& table = c_result_table;
  const Attrs& attrs = table.m_attrs;
  uint id = 0;
  // runno
  {
    const Attr& attr = attrs[id];
    attr.set_value(row, &runno, sizeof(runno));
    id++;
  }
  // name
  {
    const Attr& attr = attrs[id];
    attr.set_value(row, name, strlen(name));
    id++;
  }
  // desc
  {
    const Attr& attr = attrs[id];
    attr.set_value(row, desc, strlen(desc));
    id++;
  }
  // result
  {
    const Attr& attr = attrs[id];
    uint32 value = m_util.has_error(error);
    attr.set_value(row, &value, sizeof(value));
    id++;
  }
  // rows
  {
    const Attr& attr = attrs[id];
    attr.set_value(row, &rows, sizeof(rows));
    id++;
  }
  // reject
  {
    const Attr& attr = attrs[id];
    attr.set_value(row, &reject, sizeof(reject));
    id++;
  }
  // temperrors
  {
    const Attr& attr = attrs[id];
    attr.set_value(row, &temperrors, sizeof(temperrors));
    id++;
  }
  // runtime
  {
    const Attr& attr = attrs[id];
    attr.set_value(row, &runtime, sizeof(runtime));
    id++;
  }
  // utime
  {
    const Attr& attr = attrs[id];
    attr.set_value(row, &utime, sizeof(utime));
    id++;
  }
  // error
  set_error_attrs(row, table, error, id);
  require(id == attrs.size());
}

void
NdbImportUtil::set_reject_row(Row* row,
                              uint32 runno,
                              const Error& error,
                              const char* reject,
                              uint rejectlen)
{
  const Table& table = c_reject_table;
  const Attrs& attrs = table.m_attrs;
  uint id = 0;
  // runno
  {
    const Attr& attr = attrs[id];
    attr.set_value(row, &runno, sizeof(runno));
    id++;
  }
  // rowid
  {
    const Attr& attr = attrs[id];
    attr.set_value(row, &row->m_rowid, sizeof(row->m_rowid));
    id++;
  }
  // linenr
  {
    const Attr& attr = attrs[id];
    attr.set_value(row, &row->m_linenr, sizeof(row->m_linenr));
    id++;
  }
  // startpos
  {
    const Attr& attr = attrs[id];
    attr.set_value(row, &row->m_startpos, sizeof(row->m_startpos));
    id++;
  }
  // endpos
  {
    const Attr& attr = attrs[id];
    attr.set_value(row, &row->m_endpos, sizeof(row->m_endpos));
    id++;
  }
  // bytes
  {
    const Attr& attr = attrs[id];
    uint64 bytes = row->m_endpos - row->m_startpos;
    attr.set_value(row, &bytes, sizeof(bytes));
    id++;
  }
  // error
  set_error_attrs(row, table, error, id);
  // reject
  {
    const Attr& attr = attrs[id];
    attr.set_blob(row, reject, rejectlen);
    id++;
  }
  require(id == attrs.size());
}

void
NdbImportUtil::set_stats_row(Row* row,
                             uint32 runno,
                             const Stat& stat)
{
  const Table& table = c_stats_table;
  const Attrs& attrs = table.m_attrs;
  // floats
  double obsf = (double)stat.m_obs;
  double sum1 = stat.m_sum1;
  double sum2 = stat.m_sum2;
  uint id = 0;
  // runno
  {
    const Attr& attr = attrs[id];
    attr.set_value(row, &runno, sizeof(runno));
    id++;
  }
  // id
  {
    const Attr& attr = attrs[id];
    attr.set_value(row, &stat.m_id, sizeof(stat.m_id));
    id++;
  }
  // name
  {
    const Attr& attr = attrs[id];
    uint namelen = strlen(stat.m_name);
    attr.set_value(row, stat.m_name, namelen);
    id++;
  }
  // parent
  {
    const Attr& attr = attrs[id];
    attr.set_value(row, &stat.m_parent, sizeof(stat.m_parent));
    id++;
  }
  // obs
  {
    const Attr& attr = attrs[id];
    attr.set_value(row, &stat.m_obs, sizeof(stat.m_obs));
    id++;
  }
  // sum
  {
    const Attr& attr = attrs[id];
    attr.set_value(row, &stat.m_sum, sizeof(stat.m_sum));
    id++;
  }
  // mean
  {
    const Attr& attr = attrs[id];
    double mean = 0.0;
    if (stat.m_obs != 0)
      mean = sum1 / obsf;
    attr.set_value(row, &mean, sizeof(mean));
    id++;
  }
  // min
  {
    const Attr& attr = attrs[id];
    attr.set_value(row, &stat.m_min, sizeof(stat.m_min));
    id++;
  }
  // max
  {
    const Attr& attr = attrs[id];
    attr.set_value(row, &stat.m_max, sizeof(stat.m_max));
    id++;
  }
  // stddev
  {
    const Attr& attr = attrs[id];
    double stddev = 0.0;
    if (stat.m_obs != 0)
      stddev = ::sqrt((obsf * sum2 - (sum1 * sum1)) / (obsf * obsf));
    attr.set_value(row, &stddev, sizeof(stddev));
    id++;
  }
  require(id == attrs.size());
}

void
NdbImportUtil::set_error_attrs(Row* row,
                               const Table& table,
                               const Error& error,
                               uint& id)
{
  const Attrs& attrs = table.m_attrs;
  // errortype
  {
    const Attr& attr = attrs[id];
    const char*  errortype = error.gettypetext();
    attr.set_value(row, errortype, strlen(errortype));
    id++;
  }
  // errorcode
  {
    const Attr& attr = attrs[id];
    int32 errorcode = error.code;
    attr.set_value(row, &errorcode, sizeof(errorcode));
    id++;
  }
  // sourceline
  {
    const Attr& attr = attrs[id];
    uint32 errorline = error.line;
    attr.set_value(row, &errorline, sizeof(errorline));
    id++;
  }
  // errortext
  {
    const Attr& attr = attrs[id];
    attr.set_value(row, error.text, strlen(error.text));
    id++;
  }
}

// buf

NdbImportUtil::Buf::Buf(bool split) :
  m_split(split)
{
  m_allocptr = 0;
  m_allocsize = 0;
  m_data = 0;
  m_size = 0;
  m_top = 0;
  m_start = 0;
  m_tail = 0;
  m_len = 0;
  m_eof = false;
  m_pos = 0;
  m_lineno = 0;
}

NdbImportUtil::Buf::~Buf()
{
  delete [] m_allocptr;
}

void
NdbImportUtil::Buf::alloc(uint pagesize, uint pagecnt)
{
  require(m_allocptr == 0);
  require(pagesize != 0 && (pagesize & (pagesize - 1)) == 0);
  require(pagecnt != 0);
  uint size = pagesize * pagecnt;
  uint allocsize = size + (pagesize - 1) + 1;
  uchar* allocptr = new uchar [allocsize];
  uchar* data = allocptr;
  uint misalign = (UintPtr)data & (pagesize - 1);
  if (misalign != 0)
  {
    data += (pagesize - misalign);
    uint misalign2 = (UintPtr)data & (pagesize - 1);
    require(misalign2 == 0);
  }
  require(data + size < allocptr + allocsize);
  m_allocptr = allocptr;
  m_allocsize = allocsize;
  m_data = data;
  m_size = size;
  m_top = 0;
  m_start = 0;
  m_tail = 0;
  m_len = 0;
  if (m_split)
  {
    require(pagecnt % 2 == 0);
    m_top = size / 2;
    m_start = m_top;
  }
}

void
NdbImportUtil::Buf::copy(const uchar* src, uint len)
{
  require(m_start + m_len + len < m_allocsize);
  memcpy(&m_data[m_start + m_len], src, len);
  m_len += len;
  m_data[m_start + m_len] = 0;
}

void
NdbImportUtil::Buf::reset()
{
  m_start = 0;
  if (m_split)
  {
    require(2 * m_top == m_size);
    m_start = m_top;
  }
  m_tail = 0;
  m_len = 0;
  m_eof = false;
  m_pos = 0;
  m_lineno = 0;
}

int
NdbImportUtil::Buf::movetail(Buf& dst)
{
  require(m_tail <= m_len);
  uint bytes = m_len - m_tail;
  if (bytes > dst.m_start)
  {
    return -1;
  }
  const uchar* srcptr = &m_data[m_start + m_tail];
  uchar* dstptr = &dst.m_data[dst.m_start - bytes];
  memcpy(dstptr, srcptr, bytes);
  m_len = m_tail;
  dst.m_start -= bytes;
  dst.m_len += bytes;
  return 0;
}

NdbOut&
operator<<(NdbOut& out, const NdbImportUtil::Buf& buf)
{
  out << "allocsize=" << buf.m_allocsize;
  out << " size=" << buf.m_size;
  out << " top=" << buf.m_top;
  out << " start=" << buf.m_start;
  out << " tail=" << buf.m_tail;
  out << " len=" << buf.m_len;
  out << " eof=" << buf.m_eof;
  const uchar* dataptr = &buf.m_data[buf.m_start];
  {
    char dst[100];
    uint len = buf.m_len;
    if (len > 10)
      len = 10;
    NdbImportUtil::pretty_print(dst, &dataptr[0], len);
    out << " buf=" << "0:" << dst;
  }
  {
    char dst[100];
    require(buf.m_len >= buf.m_pos);
    uint n = buf.m_len - buf.m_pos;
    if (n > 10)
      n = 10;
    NdbImportUtil::pretty_print(dst, &dataptr[buf.m_pos], n);
    out << " pos=" << buf.m_pos << ":" << dst;
  }
  {
    out << " lineno=" << buf.m_lineno;
  }
  return out;
}

void
NdbImportUtil::pretty_print(char* dst, const void* ptr, uint len)
{
  const char* p = (const char*)ptr;
  char *s = dst;
  for (uint i = 0; i < len; i++)
  {
    int c = p[i];
    if (isascii(c) && isprint(c))
      sprintf(s, "%c", c);
    else if (c == '\n')
      sprintf(s, "%s", "\\n");
    else
      sprintf(s, "<%02X>", (uchar)c);
    s += strlen(s);
  }
}

// files

NdbImportUtil::File::File(NdbImportUtil& util, Error& error) :
  m_util(util),
  m_error(error)
{
  m_fd = -1;
  m_flags = 0;
}

NdbImportUtil::File::~File()
{
  if (m_fd != -1)
    (void)::close(m_fd);
}

int
NdbImportUtil::File::do_open(int flags)
{
  const char* path = get_path();
  require(m_fd == -1);
#ifndef _WIN32
  int fd = ::open(path, flags, Creat_mode);
#else
  int fd = ::_open(path, flags, Creat_mode);
#endif
  if (fd == -1)
  {
    const char* type = "unknown";
    if (flags == Read_flags)
      type = "read";
    if (flags == Write_flags)
      type = "write";
    if (flags == Append_flags)
      type = "append";
    m_util.set_error_os(m_error, __LINE__,
                        "%s: open for %s failed", path, type);
    return -1;
  }
  m_fd = fd;
  m_flags = flags;
  return 0;
}

int
NdbImportUtil::File::do_read(uchar* dst, uint size, uint& len)
{
  const char* path = get_path();
  require(m_fd != -1);
  len = 0;
  while (len < size)
  {
    // short read is possible on pipe
#ifndef _WIN32
    int ret = ::read(m_fd, dst + len, size - len);
#else
    int ret = ::_read(m_fd, dst + len, size - len);
#endif
    if (ret == -1)
    {
      m_util.set_error_os(m_error, __LINE__,
                          "%s: read %u bytes failed", path, size - len);
      return -1;
    }
    uint n = (uint)ret;
    if (n == 0)
      break;
    len += n;
    require(len <= size);
  }
  return 0;
}

int
NdbImportUtil::File::do_read(Buf& buf)
{
  uint dstpos = buf.m_start + buf.m_len;
  require(dstpos == buf.m_top);
  require(dstpos <= buf.m_size);
  uchar* dst = buf.m_data + dstpos;
  uint size = buf.m_size - dstpos;
  uint len = 0;
  if (do_read(dst, size, len) == -1)
    return -1;
  buf.m_eof = (len == 0);
  buf.m_len += len;
  uint endpos = buf.m_start + buf.m_len;
  require(endpos <= buf.m_size);
  require(endpos < buf.m_allocsize);
  buf.m_data[endpos] = 0;
  return 0;
}

int
NdbImportUtil::File::do_write(const uchar* src, uint size)
{
  const char* path = get_path();
  require(m_fd != -1);
#ifndef _WIN32
  int ret = ::write(m_fd, src, size);
#else
  int ret = ::_write(m_fd, src, size);
#endif
  if (ret == -1)
  {
    m_util.set_error_os(m_error, __LINE__,
                        "%s: write %u bytes failed", path, size);
    return -1;
  }
  // short write is considered error
  if (uint(ret) != size)
  {
    m_util.set_error_os(m_error, __LINE__,
                        "%s: short write %u < %u", path, (uint)ret, size);
    return -1;
  }
  return 0;
}

int
NdbImportUtil::File::do_write(const Buf& buf)
{
  const uchar* src = &buf.m_data[buf.m_start];
  uint len = buf.m_len;
  if (do_write(src, len) == -1)
    return -1;
  return 0;
}

int
NdbImportUtil::File::do_close()
{
  const char* path = get_path();
  if (m_fd == -1)
    return 0;
#ifndef _WIN32
  if (::close(m_fd) == -1)
#else
  if (::_close(m_fd) == -1)
#endif
  {
    m_util.set_error_os(m_error, __LINE__,
                        "%s: close failed", path);
    return -1;
  }
  m_fd = -1;
  return 0;
}

int
NdbImportUtil::File::do_seek(uint64 offset)
{
  const char* path = get_path();
  require(m_fd != -1);
#ifndef _WIN32
  off_t off = (off_t)offset;
  if (::lseek(m_fd, off, SEEK_SET) == -1)
#else
  __int64 off = (__int64)offset;
  if (::_lseeki64(m_fd, off, SEEK_SET) == -1)
#endif
  {
    m_util.set_error_os(m_error, __LINE__,
                        "%s: lseek %llu failed", path, offset);
    return -1;
  }
  return 0;
}

// stats

NdbImportUtil::Stat::Stat(Stats& stats,
                          uint id,
                          const char* name,
                          uint parent,
                          uint level,
                          uint flags) :
  m_stats(stats),
  m_id(id),
  m_name(name),
  m_parent(parent),
  m_level(level),
  m_flags(flags)
{
  m_childcnt = 0;
  m_firstchild = StatNULL;
  m_lastchild = StatNULL;
  m_nextchild = StatNULL;
  if (m_parent != StatNULL)
  {
    Stat* parentstat = m_stats.get(m_parent);
    if (parentstat->m_childcnt == 0)
    {
      parentstat->m_firstchild = m_id;
      parentstat->m_lastchild = m_id;
    }
    else
    {
      Stat* lastchildstat = m_stats.get(parentstat->m_lastchild);
      require(lastchildstat->m_nextchild == StatNULL);
      lastchildstat->m_nextchild = m_id;
      parentstat->m_lastchild = m_id;
    }
    parentstat->m_childcnt++;
  }
  reset();
}

void
NdbImportUtil::Stat::add(uint64 val)
{
  uint id = m_id;
  do
  {
    Stat* stat = m_stats.get(id);
    stat->m_obs++;
    stat->m_sum += val;
    if (stat->m_obs == 1)
    {
      stat->m_min = val;
      stat->m_max = val;
    }
    else
    {
      if (stat->m_min > val)
        stat->m_min = val;
      if (stat->m_max < val)
        stat->m_max = val;
    }
    stat->m_sum1 += double(val);
    stat->m_sum2 += double(val) * double(val);
    id = stat->m_parent;
    // root level stats are not useful
  } while (id != 0);
}

void
NdbImportUtil::Stat::reset()
{
  m_obs = 0;
  m_sum = 0;
  m_min = 0;
  m_max = 0;
  m_sum1 = 0.0;
  m_sum2 = 0.0;
}

NdbImportUtil::Stats::Stats(NdbImportUtil& util) :
  m_util(util)
{
  Stat* rootstat = new Stat(*this, 0, "root", StatNULL, 0, 0);
  m_stats.push_back(rootstat);
  validate();
}

NdbImportUtil::Stats::~Stats()
{
  for (uint i = 0; i < m_stats.size(); i++)
  {
    Stat* stat = get(i);
    delete stat;
  }
}

NdbImportUtil::Stat*
NdbImportUtil::Stats::create(const char* name, uint parent, uint flags)
{
  lock();
  {
    Stat* stat = find(name);
    if (stat != 0)
    {
      log2("use existing " << stat->m_name << " id=" << stat->m_id);
      unlock();
      return stat;
    }
  }
  Stat* parentstat = get(parent);
  uint parentlevel = parentstat->m_level;
  uint id = m_stats.size();
  Stat* stat = new Stat(*this, id, name, parent, parentlevel + 1, flags);
  m_stats.push_back(stat);
  log2("created stat id=" << stat->m_id << " name=" << stat->m_name);
  validate();
  unlock();
  return stat;
}

NdbImportUtil::Stat*
NdbImportUtil::Stats::get(uint i)
{
  require(i < m_stats.size());
  Stat* stat = m_stats[i];
  require(stat != 0);
  require(stat->m_id == i);
  return stat;
}

const NdbImportUtil::Stat*
NdbImportUtil::Stats::get(uint i) const
{
  require(i < m_stats.size());
  const Stat* stat = m_stats[i];
  require(stat != 0);
  require(stat->m_id == i);
  return stat;
}

NdbImportUtil::Stat*
NdbImportUtil::Stats::find(const char* name) const
{
  for (uint i = 0; i < m_stats.size(); i++)
  {
    Stat* stat = m_stats[i];
    require(stat != 0);
    if (strcmp(stat->m_name, name) == 0)
      return stat;
  }
  return 0;
}

void
NdbImportUtil::Stats::add(uint id, uint64 val)
{
  Stat* stat = get(id);
  stat->add(val);
}

const NdbImportUtil::Stat*
NdbImportUtil::Stats::next(uint id) const
{
  require(id < m_stats.size());
  const Stat* stat = m_stats[id];
  require(stat != 0);
  if (stat->m_firstchild != StatNULL)
  {
    stat = get(stat->m_firstchild);
    return stat;
  }
  while (1)
  {
    if (stat->m_nextchild != StatNULL)
    {
      stat = get(stat->m_nextchild);
      return stat;
    }
    if (stat->m_parent == StatNULL)
    {
      break;
    }
    stat = get(stat->m_parent);
  }
  return 0;
}

void
NdbImportUtil::Stats::reset()
{
  for (uint i = 0; i < m_stats.size(); i++)
  {
    Stat* stat = get(i);
    stat->reset();
  }
}

#if defined(VM_TRACE) || defined(TEST_NDBIMPORTUTIL)
void
NdbImportUtil::Stats::validate() const
{
  bool* seen = new bool [m_stats.size()];
  Validate v(StatNULL, 0, 0, seen);
  for (uint i = 0; i < m_stats.size(); i++)
    v.m_seen[i] = false;
  (void)validate(v);
  for (uint i = 0; i < m_stats.size(); i++)
    require(v.m_seen[i] == true);
  delete [] seen;
}

const NdbImportUtil::Stat*
NdbImportUtil::Stats::validate(Validate& v) const
{
  const Stat* stat = get(v.m_id);
  require(stat->m_parent == v.m_parent);
  require(stat->m_id == v.m_id);
  require(stat->m_level == v.m_level);
  const Stat* stat2 = find(stat->m_name);
  require(stat == stat2);
  require(v.m_seen[v.m_id] == false);
  v.m_seen[v.m_id] = true;
  uint sibling = stat->m_nextchild;
  if (sibling != StatNULL)
  {
    Validate v2(v.m_parent, sibling, v.m_level, v.m_seen);
    validate(v2);
  }
  uint child = stat->m_firstchild;
  if (child != StatNULL)
  {
    Validate v2(v.m_id, child, v.m_level + 1, v.m_seen);
    validate(v2);
  }
  return stat;
}
#endif

NdbOut&
operator<<(NdbOut& out, const NdbImportUtil::Stats& stats)
{
  out << "stats";
  return out;
}

// timer

NdbImportUtil::Timer::Timer()
{
  // make sure initialized to avoid assert
  start();
  stop();
}

void
NdbImportUtil::Timer::start()
{
  m_start = NdbTick_getCurrentTicks();
}

void
NdbImportUtil::Timer::stop()
{
  m_stop = NdbTick_getCurrentTicks();
  if (NdbTick_Compare(m_start, m_stop) > 0)
  {
    // not worth crashing
    m_start = m_stop;
  }
  struct ndb_rusage ru;
  if (Ndb_GetRUsage(&ru) == 0)
  {
    m_utime_msec = ru.ru_utime / 1000;
    m_stime_msec = ru.ru_stime / 1000;
  }
}

uint64
NdbImportUtil::Timer::elapsed_sec() const
{
  return NdbTick_Elapsed(m_start, m_stop).seconds();
}

uint64
NdbImportUtil::Timer::elapsed_msec() const
{
  return NdbTick_Elapsed(m_start, m_stop).milliSec();
}

uint64
NdbImportUtil::Timer::elapsed_usec() const
{
  return NdbTick_Elapsed(m_start, m_stop).microSec();
}

NdbOut&
operator<<(NdbOut& out, const NdbImportUtil::Timer& timer)
{
  double t = (double)timer.elapsed_msec() / (double)1000.0;
  char buf[100];
  sprintf(buf, "%.3f", t);
  out << buf;
  return out;
}

// error

void
NdbImportUtil::set_error_gen(Error& error, int line,
                             const char* fmt, ...)
{
  c_error_lock.lock();
  new (&error) Error;
  error.line = line;
  error.type = Error::Type_gen;
  if (fmt != 0)
  {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(error.text, sizeof(error.text), fmt, ap);
    va_end(ap);
  }
  log1("E " << error);
  if (c_opt.m_abort_on_error)
    abort();
  c_error_lock.unlock();
}

void
NdbImportUtil::set_error_usage(Error& error, int line,
                               const char* fmt, ...)
{
  c_error_lock.lock();
  new (&error) Error;
  error.line = line;
  error.type = Error::Type_usage;
  if (fmt != 0)
  {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(error.text, sizeof(error.text), fmt, ap);
    va_end(ap);
  }
  log1("E " << error);
  if (c_opt.m_abort_on_error)
    abort();
  c_error_lock.unlock();
}

void
NdbImportUtil::set_error_alloc(Error& error, int line)
{
  c_error_lock.lock();
  new (&error) Error;
  error.line = line;
  error.type = Error::Type_alloc;
  log1("E " << error);
  if (c_opt.m_abort_on_error)
    abort();
  c_error_lock.unlock();
}

void
NdbImportUtil::set_error_mgm(Error& error, int line,
                             NdbMgmHandle handle)
{
  c_error_lock.lock();
  new (&error) Error;
  error.line = line;
  error.type = Error::Type_mgm;
  error.code = ndb_mgm_get_latest_error(handle);
  snprintf(error.text, sizeof(error.text),
           "%s", ndb_mgm_get_latest_error_msg(handle));
  log1("E " << error);
  if (c_opt.m_abort_on_error)
    abort();
  c_error_lock.unlock();
}

void
NdbImportUtil::set_error_con(Error& error, int line,
                             const Ndb_cluster_connection* con)
{
  c_error_lock.lock();
  new (&error) Error;
  error.line = line;
  error.type = Error::Type_con;
  error.code = con->get_latest_error();
  snprintf(error.text, sizeof(error.text), "%s", con->get_latest_error_msg());
  log1("E " << error);
  if (c_opt.m_abort_on_error)
    abort();
  c_error_lock.unlock();
}

void
NdbImportUtil::set_error_ndb(Error& error, int line,
                             const NdbError& ndberror, const char* fmt, ...)
{
  c_error_lock.lock();
  new (&error) Error;
  error.line = line;
  error.type = Error::Type_ndb;
  error.code = ndberror.code;
  snprintf(error.text, sizeof(error.text), "%s", ndberror.message);
  log1("E " << error);
  if (c_opt.m_abort_on_error)
    abort();
  c_error_lock.unlock();
}

void
NdbImportUtil::set_error_os(Error& error, int line,
                            const char* fmt, ...)
{
  c_error_lock.lock();
  new (&error) Error;
  error.line = line;
  error.type = Error::Type_os;
  error.code = errno;
  if (fmt != 0)
  {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(error.text, sizeof(error.text), fmt, ap);
    va_end(ap);
  }
  {
    uint len = strlen(error.text);
    if (len < sizeof(error.text))
    {
      uint left = sizeof(error.text) - len;
      snprintf(&error.text[len], left, ": errno=%u: %s",
               errno, strerror(errno));
    }
  }
  log1("E " << error);
  if (c_opt.m_abort_on_error)
    abort();
  c_error_lock.unlock();
}

void
NdbImportUtil::set_error_data(Error& error, int line,
                              int code, const char* fmt, ...)
{
  c_error_lock.lock();
  new (&error) Error;
  error.line = line;
  error.type = Error::Type_data;
  error.code = code;
  if (fmt != 0)
  {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(error.text, sizeof(error.text), fmt, ap);
    va_end(ap);
  }
  log1("E " << error);
  if (c_opt.m_abort_on_error)
    abort();
  c_error_lock.unlock();
}

void
NdbImportUtil::copy_error(Error& error, const Error& error2)
{
  error.type = error2.type;
  error.code = error2.code;
  error.line = error2.line;
  memcpy(error.text, error2.text, sizeof(error.text));
}

bool NdbImportUtil::g_stop_all = false;

void
NdbImportUtil::fmt_msec_to_hhmmss(char* str, uint64 msec)
{
  uint sec = msec / 1000;
  uint hh = sec / 3600;
  sec -= hh * 3600;
  uint mm = sec / 60;
  sec -= mm * 60;
  uint ss = sec;
  uint ff = msec - sec * 1000;
  (void)ff;
  sprintf(str, "%uh%um%us", hh, mm, ss);
}

// unittest

#ifdef TEST_NDBIMPORTUTIL

typedef NdbImportUtil::ListEnt UtilListEnt;
typedef NdbImportUtil::List UtilList;
typedef NdbImportUtil::RowMap UtilRowMap;
typedef NdbImportUtil::Buf UtilBuf;
typedef NdbImportUtil::File UtilFile;
typedef NdbImportUtil::Name UtilName;
typedef NdbImportUtil::Stat UtilStat;
typedef NdbImportUtil::Stats UtilStats;

#include <NdbTap.hpp>
#include <ndb_rand.h>

static uint
myrandom()
{
  return (uint)ndb_rand() * (uint)ndb_rand();   // < 2**30
}

static uint
myrandom(uint m)
{
  require(m != 0);
  uint n = myrandom();
  return n % m;
}

struct MyRec : UtilListEnt {
  uint m_index;
  bool m_member;
  MyRec() {
    m_index = Inval_uint;
    m_member = false;
  }
};

struct MyRecs : UtilList {
};

static int
testlist()
{
  ndbout << "testlist" << endl;
  NdbImportUtil util;
  util.c_opt.m_verbose = 3;
  MyRecs recs;
  const uint poolsize = 256;
  MyRec* recpool = new MyRec [poolsize];
  for (uint n = 0; n < poolsize; n++)
  {
    MyRec* rec = &recpool[n];
    rec->m_index = n;
  }
  const uint numops = 1024 * poolsize;
  uint max_occup = 0;
  for (uint numop = 0; numop < numops; numop++)
  {
    MyRec* rec = 0;
    while (1)
    {
      uint n = myrandom(poolsize);
      rec = &recpool[n];
      if (rec->m_member)
      {
        if (myrandom(100) < 80)
          continue;
      }
      break;
    }
    if (!rec->m_member)
    {
      recs.push_back(rec);
      rec->m_member = true;
    }
    else if (myrandom(100) < 50)
    {
      MyRec* rec = static_cast<MyRec*>(recs.pop_front());
      require(rec != 0);
      rec->m_member = false;
    }
    else
    {
      recs.remove(rec);
      rec->m_member = false;
    }
    if (max_occup < recs.m_cnt)
      max_occup = recs.m_cnt;
  }
  for (MyRec* rec = static_cast<MyRec*>(recs.m_front);
      rec != 0;
      rec = static_cast<MyRec*>(rec->m_next))
  {
    uint n = rec->m_index;
    require(n < poolsize);
    require(rec == &recpool[n]);
    require(rec->m_member);
  }
  for (uint n = 0; n < poolsize; n++)
  {
    MyRec* rec = &recpool[n];
    if (rec->m_member)
    {
      recs.remove(rec);
      rec->m_member = false;
    }
  }
  require(recs.m_cnt == 0);
  delete [] recpool;
  ndbout << "max_occup=" << max_occup << endl;
  return 0;
}

static int
testrowmap()
{
  ndbout << "testrowmap" << endl;
  const uint maxranges = 10000;
  //
  ndbout << "map1: create manually" << endl;
  UtilRowMap map1;
  UtilRowMap::Ranges& ranges1 = map1.m_ranges;
  {
    uint64 start = 0;
    for (uint i = 0; i < maxranges; i++)
    {
      UtilRowMap::Range r;
      uint64 gap = myrandom(10);
      uint64 count = 1 + myrandom(10);
      r.m_start = start + gap;
      r.m_end = r.m_start + count;
      r.m_reject = myrandom(1 + count / 10);
      ranges1.push_back(r);
      start = r.m_end;
    }
  }
  ndbout << "map1: size " << ranges1.size() << endl;
  require(ranges1.size() == maxranges);
  //
  ndbout << "map2: create via merge from map1" << endl;
  UtilRowMap map2;
  UtilRowMap::Ranges& ranges2 = map2.m_ranges;
  {
    uint i = 0;
    while (i < maxranges)
    {
      UtilRowMap::Range r = ranges1[i];
      uint j = i + 1;
      while (j < maxranges)
      {
        UtilRowMap::Range r2 = ranges1[j];
        if (r.m_end < r2.m_start)
          break;
        r.m_end += r2.m_end - r2.m_start;
        r.m_reject += r2.m_reject;
        j++;
      }
      ranges2.push_back(r);
      i = j;
    }
  }
  ndbout << "map2: size " << ranges2.size() << endl;
  //
  ndbout << "map3: create via util from map1" << endl;
  UtilRowMap map3;
  UtilRowMap::Ranges& ranges3 = map3.m_ranges;
  {
    uint* reorder = new uint [maxranges];
    for (uint i = 0; i < maxranges; i++)
    {
      reorder[i] = i;
    }
    for (uint i = 0; i < maxranges; i++)
    {
      if (myrandom(10) == 0)
      {
        uint m = 100;
        if (m > maxranges - i)
          m = maxranges - i;
        uint j = i + myrandom(m);
        // std::swap
        uint k = reorder[i];
        reorder[i] = reorder[j];
        reorder[j] = k;
      }
    }
    for (uint i = 0; i < maxranges; i++)
    {
      uint j = reorder[i];
      UtilRowMap::Range r = ranges1[j];
      map3.add(r);
    }
    delete [] reorder;
  }
  ndbout << "map3: size " << ranges3.size() << endl;
  //
  ndbout << "map2 vs map3: verify" << endl;
  require(ranges2.size() == ranges3.size());
  for (uint i = 0; i < ranges2.size(); i++)
  {
    const UtilRowMap::Range r2 = ranges2[i];
    const UtilRowMap::Range r3 = ranges3[i];
    require(r2.m_start == r3.m_start);
    require(r2.m_end == r3.m_end);
    require(r2.m_reject == r2.m_reject);
  }
  //
  ndbout << "mark existing" << endl;
  struct Mark {
    Mark(uint64 size) {
      m_mark = new bool [size];
      m_size = size;
      for (uint64 i = 0; i < m_size; i++)
        m_mark[i] = false;
      m_cnt = 0;
    }
    ~Mark() {
      delete [] m_mark;
    };
    void set(uint64 pos, uint64 cnt) {
      require(pos + cnt <= m_size);
      for(uint64 i = pos; i < pos + cnt; i++) {
        require(m_mark[i] == false);
        m_mark[i] = true;
        m_cnt++;
      }
    }
    bool get(uint64 pos) {
      require(pos < m_size);
      return m_mark[pos];
    }
    void del(uint64 pos) {
      require(pos < m_size);
      require(m_mark[pos] == true);
      m_mark[pos] = false;
      require(m_cnt != 0);
      m_cnt--;
    }
    bool* m_mark;
    uint64 m_size;
    uint64 m_cnt;
  };
  uint64 maxid = ranges3.back().m_end;
  Mark mark(maxid);
  for (uint i = 0; i < ranges3.size(); i++)
  {
    const UtilRowMap::Range r3 = ranges3[i];
    mark.set(r3.m_start, r3.m_end - r3.m_start);
  }
  ndbout << "ids: size=" << mark.m_size << " cnt=" << mark.m_cnt << endl;
  //
  ndbout << "test find" << endl;
  for (uint64 id = 0; id < maxid; id++)
  {
    UtilRowMap::Iterator it;
    if (mark.get(id) == false)
      require(map3.find(id, it) == false);
    if (mark.get(id) == true)
      require(map3.find(id, it) == true);
  }
  //
  ndbout << "test remove: random 50%" << endl;
  uint64 oldcnt = mark.m_cnt;
  while (oldcnt < mark.m_cnt * 2)
  {
    uint64 id = myrandom() % mark.m_size;
    if (mark.get(id))
    {
      map3.remove(id);
      mark.del(id);
    }
  }
  //
  ndbout << "test remove: rest " << mark.m_cnt << endl;
  for (uint64 id = 0; id < mark.m_size; id++)
  {
    if (mark.get(id))
    {
      map3.remove(id);
      mark.del(id);
    }
  }
  require(map3.empty());
  require(mark.m_cnt == 0);
  return 0;
}

static int
testbuf()
{
  ndbout << "testbuf" << endl;
  uint loops = 128 * 1024;
  uint min_off = UINT_MAX;
  uint max_off = 0;
  for (uint n = 0; n < loops; n++)
  {
    UtilBuf* buf = new UtilBuf;
    uint pagesize_log2 = 1 + myrandom(15);
    uint pagesize = (1 << pagesize_log2);
    uint pagecnt = 1 + myrandom(1024);
    buf->alloc(pagesize, pagecnt);
    uint off = buf->m_data - buf->m_allocptr;
    if (min_off > off)
      min_off = off;
    if (max_off < off)
      max_off = off;
    delete buf;
  }
  ndbout << "min_off=" << min_off << " max_off=" << max_off << endl;
  return 0;
}

static int
testprint()
{
  ndbout << "testprint" << endl;
  uchar buf[256];
  char dst[5 * 256];
  for (uint i = 0; i < 256; i++)
    buf[i] = i;
  NdbImportUtil::pretty_print(dst, buf, 256);
  ndbout << dst << endl;
  return 0;
}

static int
testfile()
{
  ndbout << "testfile" << endl;
  NdbImportUtil util;
  const char* path = "test.csv";
  struct stat st;
  if (stat(path, &st) == -1)
  {
    ndbout << path << ": skip on errno " << errno << endl;
    return 0;
  }
  for (int split = 0; split <= 1; split++)
  {
    ndbout << "read " << path << " buf split=" << split << endl;
    UtilBuf buf(split);
    buf.alloc(4096, 8);
    UtilFile file(util, util.c_error);
    file.set_path(path);
    require(file.do_open(UtilFile::Read_flags) == 0);
    uint totlen = 0;
    uint totread = 0;
    while (1)
    {
      buf.reset();
      int ret = file.do_read(buf);
      require(ret == 0);
      if (buf.m_eof)
        break;
      totlen += buf.m_len;
      totread++;
    }
    require(totlen == st.st_size);
    ndbout << "len=" << totlen << " reads=" << totread << endl;
    require(file.do_close() == 0);
  }
  return 0;
}

static int
teststat()
{
  ndbout << "teststat" << endl;
  NdbImportUtil util;
  util.c_opt.m_verbose = 3;
  UtilStats stats(util);
  static const uint stattot = 256;
  uint statcnt = stats.m_stats.size();
  require(statcnt == 1);
  {
    const UtilStat* stat = stats.find("root");
    require(stat != 0);
    require(stat->m_id == 0);
  }
  for (uint i = 1; i < stattot; i++)
  {
    UtilName name("test", i);
    uint parent = myrandom(statcnt);
    uint flags = 0;
    const UtilStat* stat = stats.create(name, parent, flags);
    require(stat != 0);
    require(stat->m_id == statcnt);
    require(strcmp(stat->m_name, name) == 0);
    statcnt++;
    require(statcnt == stats.m_stats.size());
  }
  require(statcnt == stattot);
  for (uint k = 0; k < 10 * stattot; k++)
  {
    uint i = myrandom(statcnt);
    require(i < statcnt);
    UtilStat* stat = stats.get(i);
    require(stat != 0);
    if (i == 0)
    {
      UtilName name("root");
      require(strcmp(stat->m_name, name) == 0);
      UtilStat* stat2 = stats.find(name);
      require(stat == stat2);
    }
    else
    {
      UtilName name("test", i);
      require(strcmp(stat->m_name, name) == 0);
      UtilStat* stat2 = stats.find(name);
      require(stat == stat2);
      uint v =myrandom();
      stat->add(v);
    }
  }
  // iter
  bool iterseen[stattot];
  for (uint i = 0; i < stattot; i++)
    iterseen[i] = false;
  // root is skipped
  const UtilStat* stat = stats.next(0);
  iterseen[0] = true;
  uint itercnt = 1;
  while (stat != 0)
  {
    itercnt++;
    require(itercnt <= statcnt);
    const uint i = stat->m_id;
    require(i < stattot);
    require(iterseen[i] == false);
    iterseen[i] = true;
    for (uint j = i; j != 0; )
    {
      const UtilStat* stat = stats.get(j);
      require(stat != 0);
      require(iterseen[j] == true);
      ndbout << j;
      j = stat->m_parent;
      if (j != 0)
        ndbout << " ";
      else
        ndbout << endl;
    }
    stat = stats.next(i);
  }
  for (uint i = 0; i < stattot; i++)
    require(iterseen[i] == true);
  require(itercnt == statcnt);
  return 0;
}

static int
testmain()
{
  ndb_init();
#ifdef VM_TRACE
  signal(SIGABRT, SIG_DFL);
  signal(SIGSEGV, SIG_DFL);
#endif
  uint seed = (uint)NdbHost_GetProcessId();
  ndbout << "seed=" << seed << endl;
  ndb_srand(seed);
  if (testlist() != 0)
    return -1;
  if (testrowmap() != 0)
    return -1;
  if (testbuf() != 0)
    return -1;
  if (testfile() != 0)
    return -1;
  if (testprint() != 0)
    return -1;
  if (teststat() != 0)
    return -1;
  return 0;
}

TAPTEST(NdbImportUtil)
{
  int ret = testmain();
  return (ret == 0);
}

#endif
