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

#ifndef ODBC_HANDLES_AttrArea_hpp
#define ODBC_HANDLES_AttrArea_hpp

#include <map>
#include <common/common.hpp>
#include "OdbcData.hpp"

class HandleBase;

enum AttrMode {
    Attr_mode_undef,
    Attr_mode_readonly,
    Attr_mode_writeonly,
    Attr_mode_readwrite
};

/**
 * @struct AttrSpec
 * @brief Attribute specifier
 *
 * Each handle class has a list of attribute specifiers.
 */
struct AttrSpec {
    int m_id;			// SQL_ATTR_ identifier
    OdbcData::Type m_type;	// data type
    AttrMode m_mode;		// access mode, undef indicates end of list
    /**
     * Callback for checks and side effects.  Called before the
     * attribute is stored.  May set error status.
     */
    typedef void CallbackSet(Ctx& ctx, HandleBase* self, const OdbcData& data);
    CallbackSet* m_set;
    /**
     * Callback to set default value.  May set error status.
     */
    typedef void CallbackDefault(Ctx& ctx, HandleBase* self, OdbcData& data);
    CallbackDefault* m_default;
};

/**
 * @class AttrField
 * @brief Attribute value (stored as OdbcData)
 */
class AttrField {
public:
    AttrField(const AttrSpec& attrSpec, const OdbcData& data);
    AttrField(const AttrField& field);
    ~AttrField();
    void setData(const OdbcData& data);
    const OdbcData& getData();
private:
    const AttrSpec& m_spec;
    OdbcData m_data;
};

inline
AttrField::AttrField(const AttrSpec& spec, const OdbcData& data) :
    m_spec(spec),
    m_data(data)
{
}

inline
AttrField::AttrField(const AttrField& field) :
    m_spec(field.m_spec),
    m_data(field.m_data)
{
}

inline
AttrField::~AttrField()
{
}

inline void
AttrField::setData(const OdbcData& data)
{
    ctx_assert(m_spec.m_type == data.type());
    m_data.setValue(data);
}

inline const OdbcData&
AttrField::getData()
{
    ctx_assert(m_data.type() != OdbcData::Undef);
    return m_data;
}

/**
 * @class AttrArea
 * @brief Handle attributes
 * 
 * Each handle instance has a list of attribute values stored
 * under an AttrArea.  Callbacks to handle code provide for
 * default values, extra checks, and side-effects.
 */
class AttrArea {
public:
    AttrArea(const AttrSpec* specList);
    ~AttrArea();
    void setHandle(HandleBase* handle);
    const AttrSpec& findSpec(int id) const;
    void setAttr(Ctx& ctx, int id, const OdbcData& data);
    void getAttr(Ctx& ctx, int id, OdbcData& data);
private:
    HandleBase* m_handle;
    const AttrSpec* const m_specList;
    typedef std::map<int, AttrField> Fields;
    Fields m_fields;
};

inline void
AttrArea::setHandle(HandleBase* handle)
{
    ctx_assert(handle != 0);
    m_handle = handle;
}

#endif
