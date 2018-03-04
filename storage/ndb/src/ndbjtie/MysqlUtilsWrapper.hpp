/*
 Copyright (c) 2010, 2013, Oracle and/or its affiliates. All rights reserved.

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License, version 2.0,
 as published by the Free Software Foundation.

 This program is also distributed with certain software (including
 but not limited to OpenSSL) that is licensed under separate terms,
 as designated in a particular file or component or in included license
 documentation.  The authors of MySQL hereby grant you an additional
 permission to link the program and your derivative works with the
 separately licensed software that they have included with MySQL.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License, version 2.0, for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/
/*
 * MysqlUtilsWrapper.hpp
 */

#ifndef MysqlUtilsWrapper_hpp
#define MysqlUtilsWrapper_hpp

// API to wrap
#include "CharsetMap.hpp"
#include "decimal_utils.hpp"
#include "dbug_utils.hpp"

struct MysqlUtilsWrapper {

// ---------------------------------------------------------------------------

// mapped by "com_mysql_ndbjtie_mysql_CharsetMap.h"

    static const char *
    CharsetMap__getName
    ( const CharsetMap & obj, int p0 )
    {
        return obj.getName(p0);
    }
    
    static const char *
    CharsetMap__getMysqlName
    ( const CharsetMap & obj, int p0 )
    {
        return obj.getMysqlName(p0);
    }
    
    static int
    CharsetMap__getCharsetNumber
    ( const CharsetMap & obj, const char * p0 )
    {
        return obj.getCharsetNumber(p0);
    }
    
    static int
    CharsetMap__getUTF8CharsetNumber
    ( const CharsetMap & obj )
    {
        return obj.getUTF8CharsetNumber();
    }
    
    static int
    CharsetMap__getUTF16CharsetNumber
    ( const CharsetMap & obj )
    {
        return obj.getUTF16CharsetNumber();
    }
    
    static const bool *  
    CharsetMap__isMultibyte
    ( const CharsetMap & obj, int p0 )
    {
       return obj.isMultibyte(p0);
    }
    
    static CharsetMap::RecodeStatus
    CharsetMap__recode
    ( const CharsetMap & obj, Int32 * p0, int p1, int p2, const void * p3, void * p4 )
    {
#ifdef assert
        assert(sizeof(int32_t) == sizeof(Int32));
#endif
        return obj.recode((Int32*)p0, p1, p2, p3, p4);
    }
    
// ---------------------------------------------------------------------------

// mapped by "com_mysql_ndbjtie_mysql_Utils.h"

    static int
    decimal_str2bin
    ( const char * p0, int p1, int p2, int p3, void * p4, int p5 )
    {
        return ::decimal_str2bin(p0, p1, p2, p3, p4, p5);
    }
    
    static int
    decimal_bin2str
    ( const void * p0, int p1, int p2, int p3, char * p4, int p5 )
    {
        return ::decimal_bin2str(p0, p1, p2, p3, p4, p5);
    }

    static void
    dbugPush
    ( const char * p0 )
    {
        ::dbugPush(p0);
    }

    static void
    dbugPop
    ()
    {
        ::dbugPop();
    }

    static void
    dbugSet
    ( const char * p0 )
    {
        ::dbugSet(p0);
    }

    static const char *
    dbugExplain
    ( char * p0, int p1 )
    {
        return ::dbugExplain(p0, p1);
    }

    static void
    dbugPrint
    ( const char * p0, const char * p1 )
    {
        ::dbugPrint(p0, p1);
    }

// ---------------------------------------------------------------------------

};

#endif // MysqlUtilsWrapper_hpp
