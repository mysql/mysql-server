/*
 Copyright (C) 2009 Sun Microsystems, Inc.
 All rights reserved. Use is subject to license terms.

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; version 2 of the License.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/
/*
 * Ndb_cluster_connection.java
 */

package ndbjtie;

public class Ndb_cluster_connection extends jtie.Wrapper {

    // this c'tor may me protected, for access from JNI is still possible
    protected Ndb_cluster_connection(long cdelegate) {
        super(cdelegate);
        System.out.println("<-> ndbjtie.Ndb_cluster_connection(long)");
    }

    // constructor
    static public native Ndb_cluster_connection create(String connectstring);

    // destructor
    static public native void delete(Ndb_cluster_connection obj);

    public final native int connect(int no_retries, int retry_delay_in_seconds, int verbose);

    public final native int wait_until_ready(int timeout_for_first_alive, int timeout_after_first_alive);
}
