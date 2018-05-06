/*
   Copyright (c) 2017, 2018, Oracle and/or its affiliates. All rights reserved.

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

#ifndef NDB_DUMMY_TS_H
#define NDB_DUMMY_TS_H

// Implementing the callbacks for working with SDI
// in tablespaces as defined in handler.h
#include "handler.h"

// These are dummy callback implementations as there is no
// point in keeping .SDI files for each table stored in
// the data directory of MySQL Server. The tables and
// their metadata is safely stored in the transactional
// dictionary of NDB

namespace ndb_dummy_ts {

  /**
    Create SDI in a tablespace. This API should be used when
    upgrading a tablespace with no SDI.
    @param[in,out]	tablespace	tablespace object
    @retval		false		success
    @retval		true		failure
  */
  static
  bool sdi_create(dd::Tablespace*)
  {
    DBUG_ASSERT(false); // Never called
    return false; // Success
  }


  /**
    Drop SDI in a tablespace. This API should be used only
    when SDI is corrupted.
    @param[in,out]	tablespace	tablespace object
    @retval		false		success
    @retval		true		failure
  */
  static
  bool sdi_drop(dd::Tablespace*)
  {
    DBUG_ASSERT(false); // Never called
    return false; // Success
  }


  /**
    Get the SDI keys in a tablespace into the vector provided.
    @param[in]	tablespace	tablespace object
    @param[in,out]	vector		vector to hold SDI keys
    @retval		false		success
    @retval		true		failure
  */
  static
  bool sdi_get_keys(const dd::Tablespace&, dd::sdi_vector_t&)
  {
    DBUG_ASSERT(false); // Never called
    return false; // Success
  }


  /** Retrieve SDI from tablespace
    @param[in]	tablespace	tablespace object
    @param[in]	sdi_key		SDI key
    @param[in,out]	sdi		SDI retrieved from tablespace
    @param[in,out]	sdi_len		in:  size of memory allocated
                                out: actual length of SDI
    @retval		false		success
    @retval		true		failure
  */
  static
  bool sdi_get(const dd::Tablespace&, const dd::sdi_key_t*,
               void*, uint64*)
  {
    DBUG_ASSERT(false); // Never called
    return false; // Success
  }


  /** Insert/Update SDI in tablespace
    @param[in]	hton		handlerton object
    @param[in]	tablespace	tablespace object
    @param[in]	table		table object
    @param[in]	sdi_key		SDI key to uniquely identify the tablespace
                                object
    @param[in]	sdi		SDI to be stored in tablespace
    @param[in]	sdi_len		SDI length
    @retval		false		success
    @retval		true		failure
  */
  static
  bool sdi_set(handlerton *hton, const dd::Tablespace&, const dd::Table*,
               const dd::sdi_key_t*, const void*, uint64)
  {
    return false; // Success
  }


  /**
    Delete SDI from tablespace
    @param[in]	tablespace	tablespace object
    @param[in]	table		table object
    @param[in]	sdi_key		SDI key to uniquely identify the tablespace
                                  object
    @retval		false		success
    @retval		true		failure
  */
  static
  bool sdi_delete(const dd::Tablespace&, const dd::Table*,
                  const dd::sdi_key_t*)
  {
    return false; // Success
  }
}

#endif
