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

#ifndef ODBC_COMMON_ResultArea_hpp
#define ODBC_COMMON_ResultArea_hpp

#include <common/common.hpp>
#include <codegen/Code_base.hpp>

class SqlRow;

/**
 * @class ResultArea
 * @brief Execution result
 * 
 * ResultArea contains general results from executing SQL
 * statements.  Data rows from queries are fetched via derived
 * class ResultSet.
 */
class ResultArea {
public:
    ResultArea();
    virtual ~ResultArea();
    /**
     * Get number of rows:
     *
     * - for queries: number of rows fetched so far
     * - for DML statements: number of rows affected
     * - for DDL and other statements: 0
     */
    CountType getCount() const;
protected:
    void setCount(CountType count);
    void addCount(unsigned count = 1);
private:
    CountType m_count;
};

inline
ResultArea::ResultArea() :
    m_count(0)
{
}

inline CountType
ResultArea::getCount() const
{
    return m_count;
}

inline void
ResultArea::setCount(CountType count)
{
    m_count = count;
}

inline void
ResultArea::addCount(unsigned count)
{
    m_count += count;
}

/**
 * @class ResultSet
 * @brief Data rows from queries
 *
 * ResultSet is an interface implemented by SQL queries and
 * virtual queries (such as SQLTables).  It has an associated
 * data row accessor SqlRow.
 */
class ResultSet : public ResultArea {
public:
    ResultSet(const SqlRow& dataRow);
    virtual ~ResultSet();
    enum State {
	State_init = 1,		// before first fetch
	State_ok = 2,		// last fetch succeeded
	State_end = 3		// end of fetch or error
    };
    void initState();
    State getState() const;
    /**
     * Get data accessor.  Can be retrieved once and used after
     * each successful ResultSet::fetch().
     */
    const SqlRow& dataRow() const;
    /**
     * Try to fetch one more row from this result set.
     * - returns true if a row was fetched
     * - returns false on end of fetch
     * - returns false on error and sets error status
     * It is a fatal error to call fetch after end of fetch.
     */
    bool fetch(Ctx& ctx, Exec_base::Ctl& ctl);
protected:
    /**
     * Implementation of ResultSet::fetch() must be provided by
     * each concrete subclass.
     */
    virtual bool fetchImpl(Ctx& ctx, Exec_base::Ctl& ctl) = 0;
    const SqlRow& m_dataRow;
    State m_state;
};

inline
ResultSet::ResultSet(const SqlRow& dataRow) :
    m_dataRow(dataRow),
    m_state(State_end)		// explicit initState() is required
{
}

inline const SqlRow&
ResultSet::dataRow() const
{
    return m_dataRow;
}

inline void
ResultSet::initState()
{
    m_state = State_init;
    setCount(0);
}

inline ResultSet::State
ResultSet::getState() const
{
    return m_state;
}

inline bool
ResultSet::fetch(Ctx& ctx, Exec_base::Ctl& ctl)
{
    if (! (m_state == State_init || m_state == State_ok)) {
	// should not happen but return error instead of assert
	ctx.pushStatus(Error::Gen, "invalid fetch state %d", (int)m_state);
	m_state = State_end;
	return false;
    }
    if (fetchImpl(ctx, ctl)) {
	m_state = State_ok;
	addCount();
	return true;
    }
    m_state = State_end;
    return false;
}

#endif
