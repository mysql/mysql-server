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

#ifndef ODBC_COMMON_StmtArea_hpp
#define ODBC_COMMON_StmtArea_hpp

#include <common/common.hpp>
#include "ConnArea.hpp"
#include "StmtInfo.hpp"
#include "DescArea.hpp"

class PlanTree;
class ExecTree;

/**
 * @class StmtArea
 * @brief Public part of statement handle
 */
class StmtArea {
public:
    // state between ODBC function calls
    enum State {
	Free = 1,		// not in use
	Prepared = 2,		// statement prepared, maybe unbound
	NeedData = 3,		// executed, SQLParamData expected
	Open = 4		// cursor open
    };
    // connection area shared by all statements
    ConnArea& connArea();
    State getState() const;
    // SQL text
    const BaseString& sqlText();
    BaseString& nativeText();
    // allocate or unallocate connections if necessary
    bool useSchemaCon(Ctx& ctx, bool use);
    bool useConnection(Ctx& ctx, bool use);
    // statement info
    StmtInfo& stmtInfo();
    DescArea& descArea(DescUsage u);
    unsigned unbound() const;
    // set row count here and in diagnostics
    void setRowCount(Ctx& ctx, CountType rowCount);
    CountType getRowCount() const;
    // raw tuple count (tuples fetched from NDB)
    void resetTuplesFetched();
    void incTuplesFetched();
    CountType getTuplesFetched() const;
    // set dynamic function in StmtInfo only (at prepare)
    void setFunction(Ctx& ctx, const char* function, SQLINTEGER functionCode);
    // set dynamic function in diagnostics (at execute)
    void setFunction(Ctx& ctx);
protected:
    friend class CodeGen;
    friend class Executor;
    friend class Plan_root;
    StmtArea(ConnArea& connArea);
    ~StmtArea();
    void free(Ctx& ctx);
    ConnArea& m_connArea;
    State m_state;
    BaseString m_sqlText;
    BaseString m_nativeText;
    bool m_useSchemaCon;
    bool m_useConnection;
    StmtInfo m_stmtInfo;
    // plan tree output from parser and rewritten by analyze
    PlanTree* m_planTree;
    // exec tree output from analyze
    ExecTree* m_execTree;
    // pointers within HandleDesc allocated via HandleStmt
    DescArea* m_descArea[1+4];
    // parameters with unbound SQL type
    unsigned m_unbound;
    CountType m_rowCount;
    CountType m_tuplesFetched;
};

inline ConnArea&
StmtArea::connArea()
{
    return m_connArea;
}

inline StmtArea::State
StmtArea::getState() const
{
    return m_state;
}

inline const BaseString&
StmtArea::sqlText() {
    return m_sqlText;
}

inline BaseString&
StmtArea::nativeText() {
    return m_nativeText;
}

inline StmtInfo&
StmtArea::stmtInfo()
{
    return m_stmtInfo;
}

inline DescArea&
StmtArea::descArea(DescUsage u)
{
    ctx_assert(1 <= u && u <= 4);
    ctx_assert(m_descArea[u] != 0);
    return *m_descArea[u];
}

inline unsigned
StmtArea::unbound() const
{
    return m_unbound;
}

inline CountType
StmtArea::getRowCount() const
{
    return m_rowCount;
}

inline void
StmtArea::resetTuplesFetched()
{
    m_tuplesFetched = 0;
}

inline void
StmtArea::incTuplesFetched()
{
    m_tuplesFetched++;
}

inline CountType
StmtArea::getTuplesFetched() const
{
    return m_tuplesFetched;
}

#endif
