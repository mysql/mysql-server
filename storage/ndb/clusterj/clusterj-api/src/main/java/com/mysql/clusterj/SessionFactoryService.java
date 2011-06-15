/*
   Copyright 2010 Sun Microsystems, Inc.
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

package com.mysql.clusterj;

import java.util.Map;

/** This interface defines the service to create a SessionFactory
 * from a Map<String, String> of properties.
 */
public interface SessionFactoryService {

    /** Create or get a session factory. If a session factory with the
     * same value for PROPERTY_CLUSTER_CONNECTSTRING has already been created
     * in the VM, the existing factory is returned, regardless of whether
     * other properties of the factory are the same as specified in the Map.
     * @param props the properties for the session factory, in which the
     * keys are defined in Constants and the values describe the environment
     * @see Constants
     * @return the session factory
     */
    SessionFactory getSessionFactory(Map<String, String> props);
}
