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

#include <common/common.hpp>
#include <NdbMutex.h>
#include <NdbApi.hpp>
#include "PoolNdb.hpp"

#ifdef NDB_WIN32
static NdbMutex & ndb_mutex = * NdbMutex_Create();
#else
static NdbMutex ndb_mutex = NDB_MUTEX_INITIALIZER;
#endif

PoolNdb::PoolNdb() :
    m_cntUsed(0),
    m_cntFree(0)
{
}

PoolNdb::~PoolNdb()
{
}

Ndb*
PoolNdb::allocate(Ctx& ctx, int timeout)
{
    NdbMutex_Lock(&ndb_mutex);
    Ndb* pNdb;
    if (m_cntFree == 0) {
	pNdb = new Ndb("TEST_DB");
	pNdb->useFullyQualifiedNames(true);
	if (pNdb->init(64) < 0) {
	    ctx.pushStatus(pNdb, "init");
	    delete pNdb;
	    NdbMutex_Unlock(&ndb_mutex);
	    return 0;
	}
	if (pNdb->waitUntilReady(timeout) < 0) {
	    ctx.pushStatus(Sqlstate::_HYT00, Error::Gen, "connection timeout after %d seconds", timeout);
	    ctx.pushStatus(pNdb, "waitUntilReady");
	    delete pNdb;
	    NdbMutex_Unlock(&ndb_mutex);
	    return 0;
	}
	m_listFree.push_back(pNdb);
	m_cntFree++;
    }
    pNdb = m_listFree.front();
    m_listFree.pop_front();
    m_cntFree--;
    m_cntUsed++;
    ctx_log1(("alloc Ndb: used=%u free=%u", m_cntUsed, m_cntFree));
    NdbMutex_Unlock(&ndb_mutex);
    return pNdb;
}

void
PoolNdb::release(Ctx& ctx, Ndb* pNdb)
{
    NdbMutex_Lock(&ndb_mutex);
    m_listUsed.remove(pNdb);
    m_listFree.push_back(pNdb);
    m_cntFree++;
    m_cntUsed--;
    ctx_log1(("free Ndb: used=%u free=%u", m_cntUsed, m_cntFree));
    NdbMutex_Unlock(&ndb_mutex);
}
