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

#include <NdbApi.hpp>
#include <dictionary/DictCatalog.hpp>
#include <dictionary/DictSchema.hpp>
#include "ConnArea.hpp"

ConnArea::ConnArea() :
    m_state(Free),
    m_ndbObject(0),
    m_ndbSchemaCon(0),
    m_ndbConnection(0),
    m_useSchemaCon(0),
    m_useConnection(0),
    m_autocommit(true),
    m_uncommitted(false)
{
    // initialize connection catalog
    m_catalog = new DictCatalog(*this);
    m_schema = new DictSchema(*this, "NDB");
    m_catalog->addSchema(m_schema);
}

ConnArea::~ConnArea()
{
    delete m_catalog;
}

bool
ConnArea::useSchemaCon(Ctx& ctx, bool use)
{
    if (m_state == Free) {
	ctx.pushStatus(Sqlstate::_HY010, Error::Gen, "not connected");
	return false;
    }
    Ndb* ndb = m_ndbObject;
    ctx_assert(ndb != 0);
    NdbSchemaCon* scon = m_ndbSchemaCon;
    if (use) {
	if (scon == 0) {
	    ctx_assert(m_useSchemaCon == 0);
	    scon = ndb->startSchemaTransaction();
	    if (scon == 0) {
		ctx.pushStatus(ndb, scon, 0, "startSchemaTransaction");
		return false;
	    }
	    m_ndbSchemaCon = scon;
	}
	m_useSchemaCon++;
    } else {
	ctx_assert(scon != 0 && m_useSchemaCon != 0);
	if (--m_useSchemaCon == 0) {
	    ndb->closeSchemaTransaction(scon);
	    m_ndbSchemaCon = 0;
	}
    }
    return true;
}

bool
ConnArea::useConnection(Ctx& ctx, bool use)
{
    ctx_log3(("useConnection: count before=%u on-off=%d", m_useConnection, (int)use));
    if (m_state == Free) {
	ctx.pushStatus(Sqlstate::_HY010, Error::Gen, "not connected");
	return false;
    }
    Ndb* ndb = m_ndbObject;
    ctx_assert(ndb != 0);
    NdbConnection* tcon = m_ndbConnection;
    if (use) {
	if (tcon == 0) {
	    ctx_assert(m_useConnection == 0);
	    tcon = ndb->startTransaction();
	    if (tcon == 0) {
		ctx.pushStatus(ndb, tcon, 0, "startTransaction");
		return false;
	    }
	    m_ndbConnection = tcon;
	    m_state = Transacting;
	    ctx_log2(("transaction opened"));
	}
	m_useConnection++;
    } else {
	ctx_assert(tcon != 0 && m_useConnection != 0);
	if (--m_useConnection == 0) {
	    ndb->closeTransaction(tcon);
	    m_ndbConnection = 0;
	    m_uncommitted = false;
	    m_state = Connected;
	    ctx_log2(("transaction closed"));
	}
    }
    return true;
}
