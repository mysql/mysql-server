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

#include <vector>
#include "DescArea.hpp"

// DescField

// DescRec

void
DescRec::setField(int id, const OdbcData& data)
{
    Ctx ctx;
    setField(ctx, id, data);
    ctx_assert(ctx.ok());
}

void
DescRec::getField(int id, OdbcData& data)
{
    Ctx ctx;
    getField(ctx, id, data);
    ctx_assert(ctx.ok());
}

void
DescRec::setField(Ctx& ctx, int id, const OdbcData& data)
{
    Fields::iterator iter;
    iter = m_fields.find(id);
    if (ctx.logLevel() >= 3) {
	char buf[100];
	data.print(buf, sizeof(buf));
	ctx_log3(("set %s rec %d id %d = %s", DescArea::nameUsage(m_area->getUsage()), m_num, id, buf));
    }
    if (iter != m_fields.end()) {
	DescField& field = (*iter).second;
	field.setData(data);
	m_area->setBound(false);		// XXX could compare data values
	return;
    }
    const DescSpec& spec = m_area->findSpec(id);
    if (spec.m_pos != Desc_pos_end) {
	DescField field(spec, data);
	m_fields.insert(Fields::value_type(id, field));
	m_area->setBound(false);
	return;
    }
    ctx_assert(false);
}

void
DescRec::getField(Ctx& ctx, int id, OdbcData& data)
{
    Fields::iterator iter;
    iter = m_fields.find(id);
    if (iter != m_fields.end()) {
	DescField& field = (*iter).second;
	data.setValue(field.getData());
	return;
    }
    const DescSpec& spec = m_area->findSpec(id);
    if (spec.m_pos != Desc_pos_end) {
	data.setValue();
	return;				// XXX default value
    }
    ctx_assert(false);
}

// DescArea

DescArea::DescArea(HandleBase* handle, const DescSpec* specList) :
    m_handle(handle),
    m_specList(specList),
    m_alloc(Desc_alloc_undef),
    m_usage(Desc_usage_undef),
    m_bound(true)	// no bind necessary since empty
{
    m_header.m_area = this;
    m_header.m_num = -1;
    DescRec rec;
    rec.m_area = this;
    rec.m_num = m_recs.size();
    m_recs.push_back(rec);		// add bookmark record
    SQLSMALLINT count = 0;
    getHeader().setField(SQL_DESC_COUNT, count);
    m_bound = true;
}

DescArea::~DescArea()
{
}

const DescSpec&
DescArea::findSpec(int id)
{
    const DescSpec* p;
    for (p = m_specList; p->m_pos != Desc_pos_end; p++) {
	if (p->m_id == id)
	    break;
    }
    return *p;
}

unsigned
DescArea::getCount() const
{
    ctx_assert(m_recs.size() > 0);
    return m_recs.size() - 1;
}

void
DescArea::setCount(Ctx& ctx, unsigned count)
{
    if (m_recs.size() - 1 == count)
	return;
    ctx_log3(("set %s count %d to %d",
             DescArea::nameUsage(m_usage),
             (unsigned)(m_recs.size() - 1),
             count));
    m_recs.resize(1 + count);
    for (unsigned i = 0; i <= count; i++) {
	m_recs[i].m_area = this;
	m_recs[i].m_num = i;
    }
    getHeader().setField(SQL_DESC_COUNT, static_cast<SQLSMALLINT>(count));
}

DescRec&
DescArea::pushRecord()
{
    ctx_assert(m_recs.size() > 0);
    DescRec rec;
    rec.m_area = this;
    rec.m_num = m_recs.size();
    m_recs.push_back(rec);
    SQLSMALLINT count = m_recs.size() - 1;
    getHeader().setField(SQL_DESC_COUNT, count);
    return m_recs.back();
}

DescRec&
DescArea::getHeader()
{
    return m_header;
}

DescRec&
DescArea::getRecord(unsigned num)
{
    ctx_assert(num < m_recs.size());
    return m_recs[num];
}
