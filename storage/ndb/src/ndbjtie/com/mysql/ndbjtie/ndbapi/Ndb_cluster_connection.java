/*
  Copyright (c) 2010, 2017, Oracle and/or its affiliates. All rights reserved.

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
 * Ndb_cluster_connection.java
 */

package com.mysql.ndbjtie.ndbapi;

import java.nio.ByteBuffer;

import com.mysql.jtie.Wrapper;

public class Ndb_cluster_connection extends Wrapper implements Ndb_cluster_connectionConst
{
    public final native int get_latest_error() /*_const_*/;
    public final native String/*_const char *_*/ get_latest_error_msg() /*_const_*/;
    static public final native Ndb_cluster_connection create(String/*_const char *_*/ connectstring /*_= 0_*/);
    static public final native Ndb_cluster_connection create(String/*_const char *_*/ connectstring, int force_api_nodeid);
    static public final native void delete(Ndb_cluster_connection p0);
    public final native void set_name(String/*_const char *_*/ name);
    public final native void set_application_port(int port);
    public final native int set_timeout(int timeout_ms);
    public final native int connect(int no_retries /*_= 0_*/, int retry_delay_in_seconds /*_= 1_*/, int verbose /*_= 0_*/);
    public final native int wait_until_ready(int timeout_for_first_alive, int timeout_after_first_alive);
    public final native void lock_ndb_objects();
    public final native void unlock_ndb_objects();
    public final native int set_recv_thread_activation_threshold(int threshold);
    public final native int get_recv_thread_activation_threshold();
    public final native int set_recv_thread_cpu(short cpuid);
    public final native int unset_recv_thread_cpu();
    public final native NdbConst/*_const Ndb *_*/ get_next_ndb_object(NdbConst/*_const Ndb *_*/ p);
}
