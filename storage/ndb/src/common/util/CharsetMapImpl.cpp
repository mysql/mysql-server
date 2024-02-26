/* Copyright (c) 2010, 2023, Oracle and/or its affiliates.
 
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

#include "CharsetMapImpl.h"

#include <string.h>

#include "m_ctype.h"
#include "my_sys.h"

#define MYSQL_BINARY_CHARSET 63

/* build_map():
   Actually building the map is deferred until after my_init() etc. have 
   fully initialized mysql's strings library.  They cannot be done as part
   of static initialization. 
*/
void CharsetMapImpl::build_map() 
{   
    int cs_ucs2 = 0;
    int cs_utf16 = 0;
    int cs_utf8_3 = 0;
    int cs_utf8_4 = 0;
    
    /* ISO 8859 Charsets */
    put("latin1" , "windows-1252");     // Western Europe
    put("latin2" , "ISO-8859-2");     // Central Europe
    put("greek" , "ISO-8859-7");     
    put("hebrew", "ISO-8859-8");         
    put("latin5", "ISO-8859-9");     // Turkish
    put("latin7", "ISO-8859-13");    // Baltics
    
    /* IBM & Microsoft code pages */
    put("cp850", "IBM850");
    put("cp852", "IBM852");
    put("cp866", "IBM866");
    put("cp1250", "windows-1250");
    put("cp1251", "windows-1251");
    put("cp1256", "windows-1256");
    put("cp1257", "windows-1257");
    
    /* Asian Encodings */
    put("ujis", "EUC-JP");
    put("euckr", "EUC-KR");
    put("cp932", "windows-31j");
    put("eucjpms", "EUC_JP_Solaris");
    put("tis620", "TIS-620");
    
    /* Unicode */
    put("utf8mb3", "UTF-8");
    put("utf8mb4", "UTF-8");
    put("ucs2", "UTF-16");
    put("utf16", "UTF-16");
    put("utf32", "UTF-32");
    
    /* You could add here:
     put("filename", "UTF-8");    // No. 17: filename encoding
     ... but we're going to leave it out for now, because it should not be found
     in the database. */
    
    /* Others */
    put("hp8", "HP-ROMAN-8");
    put("swe7", "ISO646-SE");
    put("koi8r", "KOI8-R");      // Russian Cyrillic
    put("koi8u", "KOI8-U");      // Ukrainian Cyrillic
    put("macce", "MacCentralEurope");
    
    /* Build the fixed map */
    for(unsigned int i = 0 ; i < NDB_ARRAY_SIZE(mysql_charset_name) ; i++)
    {
        CHARSET_INFO *cs = get_charset(i, MYF(0));
        const char *mysql_name = nullptr;
        const char *mapped_name = nullptr;
        
        if(cs) 
        {
            mysql_name = cs->csname;
            mapped_name = get(mysql_name);
            if(! cs_ucs2 && ! strcmp(mysql_name, "ucs2"))       cs_ucs2 = i;
            if(! cs_utf16 && ! strcmp(mysql_name, "utf16"))     cs_utf16 = i;
            if(! cs_utf8_3 && ! strcmp(mysql_name, "utf8mb3"))  cs_utf8_3 = i;
            if(! cs_utf8_4 && ! strcmp(mysql_name, "utf8mb4"))  cs_utf8_4 = i;            
        }

        if(mapped_name) mysql_charset_name[i] = mapped_name;
        else            mysql_charset_name[i] = mysql_name;
    }
    
    if(cs_utf16) 
        UTF16Charset = cs_utf16;
    else if(cs_ucs2)
        UTF16Charset = cs_ucs2;
    else 
        UTF16Charset = 0;        
        
    if(cs_utf8_4) 
        UTF8Charset = cs_utf8_4;
    else if(cs_utf8_3)
        UTF8Charset = cs_utf8_3;
    else
        UTF8Charset = 0;
        
    ready = 1;    
}


const char * CharsetMapImpl::getName(int csnum)  
{
    if((csnum >= (int)NDB_ARRAY_SIZE(mysql_charset_name)) || (csnum < 0))
    {
        return nullptr;
    }
    return mysql_charset_name[csnum];
}


inline int CharsetMapImpl::hash(const char *name) const
{
    const unsigned char *p;
    unsigned int h = 0;
    
    for (p = (const unsigned char *) name ; *p != '\0' ; p++) 
        h = 27 * h + *p;
    return h % CHARSET_MAP_HASH_TABLE_SIZE;
}


void CharsetMapImpl::put(const char *name, const char *value) 
{
    unsigned int h = hash(name); 
    MapTableItem *i = & map[h];
    if(i->name)
    {
        i = new MapTableItem;
        map[h].next = i;
        collisions++;
    }

    i->name = name;
    i->value = value;
    n_items++; 
}


const char * CharsetMapImpl::get(const char *name) const
{
    unsigned int h = hash(name);    
    const MapTableItem *i = & map[h];
    if(i->name)
    {
        for( ; i ; i = i->next) 
        {
            if(! strcmp(name, i->name)) return i->value;
        }
    }
    return nullptr;
}
