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

#include "AttrArea.hpp"

// AttrSpec

// AttrField

// AttrArea

AttrArea::AttrArea(const AttrSpec* specList) :
    m_specList(specList)
{
}

AttrArea::~AttrArea()
{
}

const AttrSpec&
AttrArea::findSpec(int id) const
{
    const AttrSpec* p;
    for (p = m_specList; p->m_mode != Attr_mode_undef; p++) {
	if (p->m_id == id)
	    break;
    }
    return *p;
}

void
AttrArea::setAttr(Ctx& ctx, int id, const OdbcData& data)
{
    const AttrSpec& spec = findSpec(id);
    if (spec.m_mode == Attr_mode_undef) {
	ctx.pushStatus(Sqlstate::_HY092, Error::Gen, "undefined attribute id %d", id);
	return;
    }
    ctx_assert(spec.m_type == data.type());
    ctx_assert(spec.m_set != 0);
    spec.m_set(ctx, m_handle, data);
    if (! ctx.ok())
	return;
    Fields::iterator iter;
    if (ctx.logLevel() >= 3) {
	char buf[100];
	data.print(buf, sizeof(buf));
	ctx_log3(("attr handle 0x%x set id %d = %s", (unsigned)m_handle, id, buf));
    }
    iter = m_fields.find(id);
    if (iter != m_fields.end()) {
	AttrField& field = (*iter).second;
	field.setData(data);
	return;
    }
    AttrField field(spec, data);
    m_fields.insert(Fields::value_type(id, field));
}

void
AttrArea::getAttr(Ctx& ctx, int id, OdbcData& data)
{
    const AttrSpec& spec = findSpec(id);
    if (spec.m_mode == Attr_mode_undef) {
	ctx.pushStatus(Sqlstate::_HY092, Error::Gen, "undefined attribute id %d", id);
	return;
    }
    Fields::iterator iter;
    iter = m_fields.find(id);
    if (iter != m_fields.end()) {
	AttrField& field = (*iter).second;
	data.setValue(field.getData());
	return;
    }
    ctx_assert(spec.m_default != 0);
    spec.m_default(ctx, m_handle, data);
}
