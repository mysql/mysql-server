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

#ifndef ODBC_COMMON_SqlState_hpp
#define ODBC_COMMON_SqlState_hpp

#include <string.h>

/**
 * SQL states.
 */
class Sqlstate {
public:
    static const Sqlstate _00000;	// no state
    static const Sqlstate _01004;	// data converted with truncation
    static const Sqlstate _01S02;	// option value changed
    static const Sqlstate _07009;	// invalid descriptor index
    static const Sqlstate _08003;	// connection not open
    static const Sqlstate _21S01;	// insert value list does not match column list
    static const Sqlstate _22001;	// string data, right truncation
    static const Sqlstate _22002;	// indicator variable required but not supplied
    static const Sqlstate _22003;	// data overflow
    static const Sqlstate _22005;	// data is not numeric-literal
    static const Sqlstate _22008;	// data value is not a valid timestamp
    static const Sqlstate _22012;	// division by zero
    static const Sqlstate _24000;	// invalid cursor state
    static const Sqlstate _25000;	// invalid transaction state
    static const Sqlstate _42000;	// syntax error or access violation
    static const Sqlstate _42S02;	// base table or view not found
    static const Sqlstate _42S22;	// column not found
    static const Sqlstate _HY004;	// invalid SQL data type
    static const Sqlstate _HY009;	// invalid use of null pointer
    static const Sqlstate _HY010;	// function sequence error
    static const Sqlstate _HY011;	// attribute cannot be set now
    static const Sqlstate _HY012;	// invalid transaction operation code
    static const Sqlstate _HY014;	// limit on number of handles exceeded
    static const Sqlstate _HY019;	// non-character and non-binary data sent in pieces
    static const Sqlstate _HY024;	// invalid attribute value
    static const Sqlstate _HY090;	// invalid string or buffer length
    static const Sqlstate _HY091;	// invalid descriptor field identifier
    static const Sqlstate _HY092;	// invalid attribute/option identifier
    static const Sqlstate _HY095;	// function type out of range
    static const Sqlstate _HY096;	// information type out of range
    static const Sqlstate _HYC00;	// optional feature not implemented
    static const Sqlstate _HYT00;	// timeout expired
    static const Sqlstate _IM000;	// implementation defined
    static const Sqlstate _IM001;	// driver does not support this function
    static const Sqlstate _IM999;	// fill in the right state please
    // get the 5-char text string
    const char* state() const;
    // get code or "upgrade" existing code
    SQLRETURN getCode(SQLRETURN code = SQL_SUCCESS) const;
private:
    Sqlstate(const char* state, const SQLRETURN code);
    const char* const m_state;
    const SQLRETURN m_code;
};

inline const char*
Sqlstate::state() const
{
    return m_state;
}

inline
Sqlstate::Sqlstate(const char* state, const SQLRETURN code) :
    m_state(state),
    m_code(code)
{
}

#endif
