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

#ifndef ODBC_HANDLES_HandleDesc_hpp
#define ODBC_HANDLES_HandleDesc_hpp

#include <common/common.hpp>
#include <common/DescArea.hpp>
#include "HandleBase.hpp"

class HandleRoot;
class HandleDbc;

/**
 * @class HandleDesc
 * @brief Descriptor handle (SQLHDESC).
 */
class HandleDesc : public HandleBase {
public:
    HandleDesc(HandleDbc* pDbc);
    ~HandleDesc();
    void ctor(Ctx& ctx);
    void dtor(Ctx& ctx);
    HandleDbc* getDbc();
    HandleBase* getParent();
    HandleRoot* getRoot();
    OdbcHandle odbcHandle();
    DescArea& descArea();
    // allocate and free handles (no valid case)
    void sqlAllocHandle(Ctx& ctx, SQLSMALLINT childType, HandleBase** ppChild);
    void sqlFreeHandle(Ctx& ctx, SQLSMALLINT childType, HandleBase* pChild);
    // set and get descriptor values (internal use)
    void sqlSetDescField(Ctx& ctx, SQLSMALLINT recNumber, SQLSMALLINT fieldIdentifier, SQLPOINTER value, SQLINTEGER bufferLength);
    void sqlGetDescField(Ctx& ctx, SQLSMALLINT recNumber, SQLSMALLINT fieldIdentifier, SQLPOINTER value, SQLINTEGER bufferLength, SQLINTEGER* stringLength, SQLSMALLINT* stringLength2 = 0);
    void sqlColAttribute(Ctx& ctx, SQLUSMALLINT columnNumber, SQLUSMALLINT fieldIdentifier, SQLPOINTER characterAttribute, SQLSMALLINT bufferLength, SQLSMALLINT* stringLength, SQLPOINTER numericAttribute);
    void sqlColAttributes(Ctx& ctx, SQLUSMALLINT icol, SQLUSMALLINT fdescType, SQLPOINTER rgbDesc, SQLSMALLINT cbDescMax, SQLSMALLINT* pcbDesc, SQLINTEGER* pfDesc);
    // set and get several common descriptor values
    void sqlSetDescRec(Ctx& ctx, SQLSMALLINT recNumber, SQLSMALLINT Type, SQLSMALLINT SubType, SQLINTEGER Length, SQLSMALLINT Precision, SQLSMALLINT Scale, SQLPOINTER Data, SQLINTEGER* StringLength, SQLINTEGER* Indicator);
    void sqlGetDescRec(Ctx& ctx, SQLSMALLINT recNumber, SQLCHAR* Name, SQLSMALLINT BufferLength, SQLSMALLINT* StringLength, SQLSMALLINT* Type, SQLSMALLINT* SubType, SQLINTEGER* Length, SQLSMALLINT* Precision, SQLSMALLINT* Scale, SQLSMALLINT* Nullable);
private:
    HandleDbc* const m_dbc;
    static DescSpec m_descSpec[];
    DescArea m_descArea;
};

inline HandleDbc*
HandleDesc::getDbc()
{
    return m_dbc;
}

inline HandleBase*
HandleDesc::getParent()
{
    return (HandleDbc*)getDbc();
}

inline HandleRoot*
HandleDesc::getRoot()
{
    return getParent()->getRoot();
}

inline OdbcHandle
HandleDesc::odbcHandle()
{
    return Odbc_handle_desc;
}

inline DescArea&
HandleDesc::descArea()
{
    return m_descArea;
}

#endif
