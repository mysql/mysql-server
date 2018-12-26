/*
  Copyright (c) 2010, 2012, Oracle and/or its affiliates. All rights reserved.

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
 * NDBAPI.java
 */

package com.mysql.ndbjtie.ndbapi;

import java.nio.ByteBuffer;

import com.mysql.jtie.Wrapper;

public class NDBAPI implements NDBAPIConst
{
    // MMM unsupported, opaque MySQL server type, mapped by mysql utilities: public final native struct charset_info_st;
    // MMM unsupported, opaque MySQL server type, mapped by mysql utilities: public final native typedef struct charset_info_st CHARSET_INFO;
    static public final native boolean create_instance(Ndb_cluster_connection/*_Ndb_cluster_connection *_*/ cc, int/*_Uint32_*/ max_ndb_objects, int/*_Uint32_*/ no_conn_obj, int/*_Uint32_*/ init_no_ndb_objects);
    static public final native void drop_instance();
    static public final native Ndb/*_Ndb *_*/ get_ndb_object(int[]/*_Uint32 &_*/ hint_id, String/*_const char *_*/ a_catalog_name, String/*_const char *_*/ a_schema_name);
    static public final native void return_ndb_object(Ndb/*_Ndb *_*/ returned_object, int/*_Uint32_*/ id);
}
