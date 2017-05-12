/*
 *  Copyright (c) 2010, 2016, Oracle and/or its affiliates. All rights reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

package com.mysql.clusterj;

import java.util.List;
import java.util.Map;

/** SessionFactory represents a cluster.
 *
 */
public interface SessionFactory {

    /** Create a Session to use with the cluster, using all the
     * properties of the SessionFactory.
     * @return the session
     */
    Session getSession();

    /** Create a session to use with the cluster, overriding some properties.
     * Properties PROPERTY_CLUSTER_CONNECTSTRING, PROPERTY_CLUSTER_DATABASE,
     * and PROPERTY_CLUSTER_MAX_TRANSACTIONS may not be overridden.
     * @param properties overriding some properties for this session
     * @return the session
     */
    Session getSession(Map properties);

    /** Get a list containing the number of open sessions for each connection
     * in the connection pool.
     * @since 7.3.14, 7.4.12, 7.5.2
     */
    public List<Integer> getConnectionPoolSessionCounts();

    /** Close this session factory. Release all resources.
     * 
     */
    void close();

}
