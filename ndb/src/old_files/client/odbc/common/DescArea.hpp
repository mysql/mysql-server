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

#ifndef ODBC_COMMON_DescArea_hpp
#define ODBC_COMMON_DescArea_hpp

#include <map>
#include <vector>
#include <common/common.hpp>
#include "OdbcData.hpp"

/**
 * Descriptor records.  Contains:
 * -# header, not called a "record" in this context
 * -# bookmark record at index position 0
 * -# descriptor records at index positions starting from 1
 *
 * These classes are in common/ since the code is general.
 * However each area is associated with a HandleDesc.
 *
 * DescField - field identified by an SQL_DESC_* constant
 * DescRec   - header or record, a list of fields
 * DescArea  - header and all records
 */

class HandleBase;
class DescField;
class DescRec;
class DescArea;

enum DescPos {
    Desc_pos_undef = 0,
    Desc_pos_header,
    Desc_pos_record,
    Desc_pos_end
};

enum DescMode {
    Desc_mode_undef = 0,
    Desc_mode_readonly,
    Desc_mode_writeonly,
    Desc_mode_readwrite,
    Desc_mode_unused
};

struct DescSpec {
    DescPos m_pos;		// header or record
    int m_id;			// SQL_DESC_ identifier
    OdbcData::Type m_type;	// data type
    DescMode m_mode[1+4];	// access mode IPD APD IRD ARD
    // called before setting value
    typedef void CallbackSet(Ctx& ctx, HandleBase* self, const OdbcData& data);
    CallbackSet* m_set;
    // called to get default value
    typedef void CallbackDefault(Ctx& ctx, HandleBase* self, OdbcData& data);
    CallbackDefault* m_default;
};

enum DescAlloc {
    Desc_alloc_undef = 0,
    Desc_alloc_auto,
    Desc_alloc_user
};

enum DescUsage {
    Desc_usage_undef = 0,
    Desc_usage_IPD = 1,		// these must be 1-4
    Desc_usage_IRD = 2,
    Desc_usage_APD = 3,
    Desc_usage_ARD = 4
};

/**
 * @class DescField
 * @brief Field identified by an SQL_DESC_* constant
 */
class DescField {
public:
    DescField(const DescSpec& spec, const OdbcData& data);
    DescField(const DescField& field);
    ~DescField();
private:
    friend class DescRec;
    void setData(const OdbcData& data);
    const OdbcData& getData();
    const DescSpec& m_spec;
    OdbcData m_data;
};

inline
DescField::DescField(const DescSpec& spec, const OdbcData& data) :
    m_spec(spec),
    m_data(data)
{
}

inline
DescField::DescField(const DescField& field) :
    m_spec(field.m_spec),
    m_data(field.m_data)
{
}

inline
DescField::~DescField()
{
}

inline void
DescField::setData(const OdbcData& data)
{
    ctx_assert(m_spec.m_type == data.type());
    m_data.setValue(data);
}

inline const OdbcData&
DescField::getData()
{
    ctx_assert(m_data.type() != OdbcData::Undef);
    return m_data;
}

/**
 * @class DescRec
 * @brief Descriptor record, a list of fields
 */
class DescRec {
    friend class DescArea;
public:
    DescRec();
    ~DescRec();
    void setField(int id, const OdbcData& data);
    void getField(int id, OdbcData& data);
    void setField(Ctx& ctx, int id, const OdbcData& data);
    void getField(Ctx& ctx, int id, OdbcData& data);
private:
    DescArea* m_area;
    int m_num;		// for logging only -1 = header 0 = bookmark
    typedef std::map<int, DescField> Fields;
    Fields m_fields;
};

inline
DescRec::DescRec() :
    m_area(0)
{
}

inline
DescRec::~DescRec()
{
}

/**
 * @class DescArea
 * @brief All records, including header (record 0)
 *
 * Descriptor area includes a header (record 0)
 * and zero or more records at position >= 1.  
 * Each of these describes one parameter or one column.
 *
 * - DescArea : Collection of records
 *   - DescRec : Collection of fields
 *     - DescField : Contains data of type OdbcData
 */
class DescArea {
public:
    DescArea(HandleBase* handle, const DescSpec* specList);
    ~DescArea();
    void setAlloc(DescAlloc alloc);
    DescAlloc getAlloc() const;
    void setUsage(DescUsage usage);
    DescUsage getUsage() const;
    static const char* nameUsage(DescUsage u);
    // find specifier
    const DescSpec& findSpec(int id);
    // get or set number of records (record 0 not counted)
    unsigned getCount() const;
    void setCount(Ctx& ctx, unsigned count);
    // paush new record (record 0 exists always)
    DescRec& pushRecord();
    // get ref to header or to any record
    DescRec& getHeader();
    DescRec& getRecord(unsigned num);
    // modified since last bind
    void setBound(bool bound);
    bool isBound() const;
private:
    HandleBase* m_handle;
    const DescSpec* const m_specList;
    DescRec m_header;
    typedef std::vector<DescRec> Recs;
    Recs m_recs;
    DescAlloc m_alloc;
    DescUsage m_usage;
    bool m_bound;
};

inline void
DescArea::setAlloc(DescAlloc alloc)
{
    m_alloc = alloc;
}

inline DescAlloc
DescArea::getAlloc() const
{
    return m_alloc;
}

inline void
DescArea::setUsage(DescUsage usage)
{
    m_usage = usage;
}

inline DescUsage
DescArea::getUsage() const
{
    return m_usage;
}

inline const char*
DescArea::nameUsage(DescUsage u)
{
    switch (u) {
    case Desc_usage_undef:
	break;
    case Desc_usage_IPD:
	return "IPD";
    case Desc_usage_IRD:
	return "IRD";
    case Desc_usage_APD:
	return "APD";
    case Desc_usage_ARD:
	return "ARD";
    }
    return "?";
}

inline void
DescArea::setBound(bool bound)
{
    m_bound = bound;
}

inline bool
DescArea::isBound() const
{
    return m_bound;
}

#endif
