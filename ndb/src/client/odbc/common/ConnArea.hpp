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

#ifndef ODBC_COMMON_ConnArea_hpp
#define ODBC_COMMON_ConnArea_hpp

#include <common/common.hpp>

class Ctx;
class Ndb;
class NdbSchemaCon;
class NdbConnection;
class DictCatalog;
class DictSchema;

/**
 * @class ConnArea
 * @brief Public part of connection handle
 */
class ConnArea {
public:
    // state between ODBC function calls
    enum State {
	Free = 1,		// not in use, no Ndb object
	Connected = 2,		// has Ndb object but no Ndb connection
	Transacting = 3		// has Ndb connection
    };
    State getState() const;
    DictCatalog& dictCatalog() const;
    DictSchema& dictSchema() const;
    Ndb* ndbObject() const;
    NdbSchemaCon* ndbSchemaCon() const;
    NdbConnection* ndbConnection() const;
    // called from StmtArea
    bool useSchemaCon(Ctx& ctx, bool use);
    bool useConnection(Ctx& ctx, bool use);
    // these just get and set the flag - no semantics
    bool autocommit() const;
    void autocommit(bool flag);
    bool uncommitted() const;
    void uncommitted(bool flag);
protected:
    ConnArea();
    ~ConnArea();
    State m_state;
    DictCatalog* m_catalog;
    DictSchema* m_schema;
    Ndb* m_ndbObject;
    NdbSchemaCon* m_ndbSchemaCon;
    NdbConnection* m_ndbConnection;
    unsigned m_useSchemaCon;
    unsigned m_useConnection;
    bool m_autocommit;
    bool m_uncommitted;		// has uncommitted changes
};

inline ConnArea::State
ConnArea::getState() const
{
    return m_state;
}

inline DictCatalog&
ConnArea::dictCatalog() const
{
    ctx_assert(m_catalog != 0);
    return *m_catalog;
}

inline DictSchema&
ConnArea::dictSchema() const
{
    ctx_assert(m_schema != 0);
    return *m_schema;
}

inline Ndb*
ConnArea::ndbObject() const
{
    ctx_assert(m_ndbObject != 0);
    return m_ndbObject;
}

inline NdbSchemaCon*
ConnArea::ndbSchemaCon() const
{
    ctx_assert(m_ndbSchemaCon != 0);
    return m_ndbSchemaCon;
}

inline NdbConnection*
ConnArea::ndbConnection() const
{
    ctx_assert(m_ndbConnection != 0);
    return m_ndbConnection;
}

inline bool
ConnArea::autocommit() const
{
    return m_autocommit;
}

inline void
ConnArea::autocommit(bool flag)
{
    m_autocommit = flag;
}

inline bool
ConnArea::uncommitted() const
{
    return m_uncommitted;
}

inline void
ConnArea::uncommitted(bool flag)
{
    m_uncommitted = flag;
}

#endif
