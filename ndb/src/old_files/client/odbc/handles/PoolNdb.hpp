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

#ifndef ODBC_HANDLES_PoolNdb_hpp
#define ODBC_HANDLES_PoolNdb_hpp

#include <common/common.hpp>
#include <list>

class Ndb;

/**
 * @class PoolNdb
 * @brief Pool of Ndb objects.
 *
 * A class implementing pool of Ndb objects.
 */
class PoolNdb {
public:
    PoolNdb();
    ~PoolNdb();
    Ndb* allocate(Ctx& ctx, int timeout);
    void release(Ctx& ctx, Ndb* pNdb);
private:
    std::list<Ndb*> m_listUsed;
    std::list<Ndb*> m_listFree;
    unsigned m_cntUsed;
    unsigned m_cntFree;
};

#endif
