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

#include <common/StmtArea.hpp>
#include "Code_base.hpp"
#include "Code_root.hpp"

// Plan_base

Plan_base::~Plan_base()
{
}

StmtArea&
Plan_base::stmtArea() const
{
    ctx_assert(m_root != 0);
    return m_root->m_stmtArea;
}

DescArea&
Plan_base::descArea(DescUsage u) const
{
    return stmtArea().descArea(u);
}

ConnArea&
Plan_base::connArea() const
{
    return stmtArea().connArea();
}

DictCatalog&
Plan_base::dictCatalog() const
{
    return connArea().dictCatalog();
}

DictSchema&
Plan_base::dictSchema() const
{
    return connArea().dictSchema();
}

Ndb*
Plan_base::ndbObject() const
{
    Ndb* ndb = connArea().ndbObject();
    ctx_assert(ndb != 0);
    return ndb;
}

NdbSchemaCon*
Plan_base::ndbSchemaCon() const
{
    NdbSchemaCon* ndbSchemaCon = connArea().ndbSchemaCon();
    ctx_assert(ndbSchemaCon != 0);
    return ndbSchemaCon;
}

NdbConnection*
Plan_base::ndbConnection() const
{
    NdbConnection* ndbConnection = connArea().ndbConnection();
    ctx_assert(ndbConnection != 0);
    return ndbConnection;
}

void
Plan_base::printList(Ctx& ctx, Plan_base* a[], unsigned n)
{
    for (unsigned i = 0; i < n; i++) {
	if (a[i] == 0)
	    ctx.print(" -");
	else
	    a[i]->print(ctx);
    }
}

// Exec_base

Exec_base::Code::~Code()
{
}

Exec_base::Data::~Data()
{
}

Exec_base::~Exec_base()
{
    delete m_code;	// remove when code becomes shared
    m_code = 0;
    delete m_data;
    m_data = 0;
}

StmtArea&
Exec_base::stmtArea() const
{
    ctx_assert(m_root != 0);
    return m_root->m_stmtArea;
}

DescArea&
Exec_base::descArea(DescUsage u) const
{
    return stmtArea().descArea(u);
}

ConnArea&
Exec_base::connArea() const
{
    return stmtArea().connArea();
}

DictSchema&
Exec_base::dictSchema() const
{
    return connArea().dictSchema();
}

Ndb*
Exec_base::ndbObject() const
{
    Ndb* ndb = connArea().ndbObject();
    ctx_assert(ndb != 0);
    return ndb;
}

NdbSchemaCon*
Exec_base::ndbSchemaCon() const
{
    NdbSchemaCon* ndbSchemaCon = connArea().ndbSchemaCon();
    ctx_assert(ndbSchemaCon != 0);
    return ndbSchemaCon;
}

NdbConnection*
Exec_base::ndbConnection() const
{
    NdbConnection* ndbConnection = connArea().ndbConnection();
    ctx_assert(ndbConnection != 0);
    return ndbConnection;
}

void
Exec_base::printList(Ctx& ctx, Exec_base* a[], unsigned n)
{
    for (unsigned i = 0; i < n; i++) {
	ctx_assert(a[i] != 0);
	a[i]->print(ctx);
    }
}
