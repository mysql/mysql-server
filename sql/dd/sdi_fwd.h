/* Copyright (c) 2014, 2016, Oracle and/or its affiliates. All rights reserved.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef DD_SDI_FWD_H_INCLUDED
#define	DD_SDI_FWD_H_INCLUDED

/**
  @file
  @ingroup sdi
  This header provides @ref rj_template_fwd which are needed to
  create @ref dd_rj_type_alias
*/

/**
  @defgroup rj_template_fwd Rapidjson Template Declarations
  @ingroup sdi
  Inject template declarations for rapidjson
  templates for which we would like to forward declare an
  instantiation. This is needed as the rapidjson code does not provide
  these in its own headers.

  @{
*/

/**
  @namespace rapidjson
  We take the liberty of injecting some declarations into this
  namespace as the rapidjson code doesn't provide its own headers for
  forward declarations.
*/
namespace rapidjson {
class CrtAllocator;

template <typename>
class MemoryPoolAllocator;

template <typename>
struct UTF8;

template <typename, typename>
class GenericValue;

template <typename, typename, typename>
class GenericDocument;

template <typename, typename>
class GenericStringBuffer;

template <typename, typename, typename, typename, unsigned>
class PrettyWriter;
}
/** @} */ // rj_template_fwd


/**
  @defgroup dd_rj_type_alias Rapidjson Type Aliases
  @ingroup sdi

  Create type aliases for rapidjson template instantiations which will
  be used by (de)serialization code.

  @{
*/
namespace dd {
typedef rapidjson::UTF8<char> RJ_Encoding;
typedef rapidjson::MemoryPoolAllocator<rapidjson::CrtAllocator> RJ_Allocator;
typedef rapidjson::GenericDocument<RJ_Encoding, RJ_Allocator, rapidjson::CrtAllocator> RJ_Document;
typedef rapidjson::GenericValue<RJ_Encoding, RJ_Allocator> RJ_Value;
typedef rapidjson::GenericStringBuffer<RJ_Encoding, rapidjson::CrtAllocator>
RJ_StringBuffer;
typedef rapidjson::PrettyWriter<RJ_StringBuffer, RJ_Encoding, RJ_Encoding,
                                RJ_Allocator, 0> RJ_PrettyWriter;

typedef RJ_PrettyWriter Sdi_writer;
class Sdi_rcontext;
class Sdi_wcontext;
}
/** @} */ // dd_rj_type_alias

#endif	/* DD_SDI_FWD_H_INCLUDED */
