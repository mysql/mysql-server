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

#ifndef ODBC_COMMON_DiagArea_hpp
#define ODBC_COMMON_DiagArea_hpp

#include <map>
#include <vector>
#include <common/common.hpp>
#include "OdbcData.hpp"

enum DiagPos {
    Diag_pos_undef = 0,
    Diag_pos_header,
    Diag_pos_status,
    Diag_pos_end
};

/**
 * @class DiagSpec
 * @brief Field specification
 */
struct DiagSpec {
    DiagPos m_pos;		// header or status
    int m_id;			// SQL_DIAG_ identifier
    OdbcData::Type m_type;	// data type
    unsigned m_handles;		// defined for these handle types
    // find the spec
    static const DiagSpec& find(int id);
};

/**
 * @class DiagField
 * @brief Field identified by an SQL_DIAG_* constant
 */
class DiagField {
public:
    DiagField(const DiagSpec& spec, const OdbcData& data);
    DiagField(const DiagField& field);
    ~DiagField();
    void setData(const OdbcData& data);
    const OdbcData& getData();
private:
    const DiagSpec& m_spec;
    OdbcData m_data;
};

inline
DiagField::DiagField(const DiagSpec& spec, const OdbcData& data) :
    m_spec(spec),
    m_data(data)
{
}

inline
DiagField::DiagField(const DiagField& field) :
    m_spec(field.m_spec),
    m_data(field.m_data)
{
}

inline
DiagField::~DiagField()
{
}

inline void
DiagField::setData(const OdbcData& data)
{
    ctx_assert(m_spec.m_type == data.type());
    m_data.setValue(data);
}

inline const OdbcData&
DiagField::getData()
{
    ctx_assert(m_data.type() != OdbcData::Undef);
    return m_data;
}

/**
 * @class DiagRec
 * @brief One diagnostic record, a list of fields
 */
class DiagRec {
public:
    DiagRec();
    ~DiagRec();
    void setField(int id, const OdbcData& data);
    void getField(Ctx& ctx, int id, OdbcData& data);
private:
    typedef std::map<int, DiagField> Fields;
    Fields m_fields;
};

inline
DiagRec::DiagRec()
{
}

inline
DiagRec::~DiagRec()
{
}

/**
 * @class DiagArea
 * @brief All records, including header (record 0)
 *
 * Diagnostic area includes a header (record 0) and zero or more
 * status records at positions >= 1.
 */
class DiagArea {
public:
    DiagArea();
    ~DiagArea();
    /**
     * Get number of status records.
     */
    unsigned numStatus();
    /**
     * Push new status record.
     */
    void pushStatus();
    /**
     * Set field in header.
     */
    void setHeader(int id, const OdbcData& data);
    /**
     * Set field in latest status record.  The arguments can
     * also be plain int, char*, Sqlstate.  The NDB and other
     * native errors set Sqlstate _IM000 automatically.
     */
    void setStatus(int id, const OdbcData& data);
    void setStatus(const Sqlstate& state);
    void setStatus(const Error& error);
    /**
     * Convenience methods to push new status record and set
     * some common fields in it.  Sqlstate is set always.
     */
    void pushStatus(const Error& error);
    /**
     * Get refs to various records.
     */
    DiagRec& getHeader();
    DiagRec& getStatus();
    DiagRec& getRecord(unsigned num);
    /**
     * Get diag values.
     */
    void getRecord(Ctx& ctx, unsigned num, int id, OdbcData& data);
    /**
     * Get or set return code.
     */
    SQLRETURN getCode() const;
    void setCode(SQLRETURN ret);
    /**
     * Get "next" record number (0 when no more).
     * Used only by the deprecated SQLError function.
     */
    unsigned nextRecNumber();
private:
    typedef std::vector<DiagRec> Recs;
    Recs m_recs;
    SQLRETURN m_code;
    unsigned m_recNumber;	// for SQLError
};

inline SQLRETURN
DiagArea::getCode() const
{
    return m_code;
}

inline unsigned
DiagArea::nextRecNumber()
{
    if (m_recNumber >= numStatus())
	return 0;
    return ++m_recNumber;
}

#endif
