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

#include "DataRow.hpp"

// SqlSpecs

SqlSpecs::SqlSpecs(unsigned count) :
    m_count(count)
{
    m_sqlSpec = new SqlSpec[1 + count];
}

SqlSpecs::SqlSpecs(const SqlSpecs& sqlSpecs) :
    m_count(sqlSpecs.m_count)
{
    m_sqlSpec = new SqlSpec[1 + m_count];
    for (unsigned i = 1; i <= m_count; i++) {
	void* place = static_cast<void*>(&m_sqlSpec[i]);
	new (place) SqlSpec(sqlSpecs.m_sqlSpec[i]);
    }
}

SqlSpecs::~SqlSpecs()
{
    delete[] m_sqlSpec;
}

// ExtSpecs

ExtSpecs::ExtSpecs(unsigned count) :
    m_count(count)
{
    m_extSpec = new ExtSpec[1 + count];
}

ExtSpecs::ExtSpecs(const ExtSpecs& extSpecs) :
    m_count(extSpecs.m_count)
{
    m_extSpec = new ExtSpec[1 + m_count];
    for (unsigned i = 1; i <= m_count; i++) {
	void* place = static_cast<void*>(&m_extSpec[i]);
	new (place) ExtSpec(extSpecs.m_extSpec[i]);
    }
}

ExtSpecs::~ExtSpecs()
{
    delete[] m_extSpec;
}

// SqlRow

SqlRow::SqlRow(const SqlSpecs& sqlSpecs) :
    m_sqlSpecs(sqlSpecs)
{
    m_sqlField = new SqlField[1 + count()];
    for (unsigned i = 1; i <= count(); i++) {
	SqlField sqlField(m_sqlSpecs.getEntry(i));
	setEntry(i, sqlField);
    }
}

SqlRow::SqlRow(const SqlRow& sqlRow) :
    m_sqlSpecs(sqlRow.m_sqlSpecs)
{
    m_sqlField = new SqlField[1 + count()];
    for (unsigned i = 1; i <= count(); i++) {
	void* place = static_cast<void*>(&m_sqlField[i]);
	new (place) SqlField(sqlRow.getEntry(i));
    }
}

SqlRow::~SqlRow()
{
    for (unsigned i = 1; i <= count(); i++) {
	m_sqlField[i].~SqlField();
    }
    delete[] m_sqlField;
}

SqlRow*
SqlRow::copy() const
{
    SqlRow* copyRow = new SqlRow(m_sqlSpecs);
    for (unsigned i = 1; i <= count(); i++) {
	const SqlField* sqlField = &m_sqlField[i];
	while (sqlField->sqlSpec().store() == SqlSpec::Reference) {
	    sqlField = sqlField->u_data.m_sqlField;
	}
	copyRow->setEntry(i, *sqlField);
    }
    return copyRow;
}

void
SqlRow::copyout(Ctx& ctx, class ExtRow& extRow) const
{
    for (unsigned i = 1; i <= count(); i++) {
	const SqlField& sqlField = getEntry(i);
	ExtField& extField = extRow.getEntry(i);
	sqlField.copyout(ctx, extField);
    }
}

// ExtRow

ExtRow::ExtRow(const ExtSpecs& extSpecs) :
    m_extSpecs(extSpecs)
{
    m_extField = new ExtField[1 + count()];
}

ExtRow::ExtRow(const ExtRow& extRow) :
    m_extSpecs(extRow.m_extSpecs)
{
    m_extField = new ExtField[1 + count()];
    for (unsigned i = 1; i <= count(); i++) {
	void* place = static_cast<void*>(&m_extField[i]);
	new (place) ExtField(extRow.getEntry(i));
    }
}

ExtRow::~ExtRow()
{
    delete[] m_extField;
}
