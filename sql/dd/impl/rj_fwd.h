/* Copyright (c) 2014, 2015, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */

#ifndef DD_RJ_DESERIAL_FWD_H_INCLUDED
#define	DD_RJ_DESERIAL_FWD_H_INCLUDED

namespace rapidjson {
    class CrtAllocator;

    template <typename>
    class MemoryPoolAllocator;

    template <typename>
    struct UTF8;

    template <typename, typename>
    class GenericValue;

    template <typename, typename>
    class GenericStringBuffer;

    template <typename, typename, typename, typename>
    class Writer;

    template <typename, typename, typename, typename>
    class PrettyWriter;
}

namespace dd {
    typedef rapidjson::UTF8<char> RJ_Encoding;
    typedef rapidjson::MemoryPoolAllocator<rapidjson::CrtAllocator> RJ_Allocator;
    typedef rapidjson::GenericValue<RJ_Encoding, RJ_Allocator > RJ_Document;
    typedef rapidjson::GenericStringBuffer<RJ_Encoding, rapidjson::CrtAllocator>
    RJ_StringBuffer;
    typedef rapidjson::Writer<RJ_StringBuffer, RJ_Encoding, RJ_Encoding,
            RJ_Allocator> RJ_Writer;
    typedef rapidjson::PrettyWriter<RJ_StringBuffer, RJ_Encoding, RJ_Encoding,
            RJ_Allocator> RJ_PrettyWriter;

    /* Temporary until proper variant support is added */
    typedef RJ_PrettyWriter WriterVariant;
}

#endif	/* DD_RJ_DESERIAL_FWD_H_INCLUDED */

