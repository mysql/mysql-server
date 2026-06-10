/*
   Copyright (c) 2025, 2026, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

package com.mysql.clusterj.core;

import com.mysql.clusterj.Constants;
import com.mysql.clusterj.Session;
import com.mysql.clusterj.SessionFactory;

import com.mysql.clusterj.core.SessionFactoryImpl.Spec;

import java.util.ArrayList;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.ConcurrentMap;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/*  MultiDbSessionFactory

    This is an "umbrella" SessionFactory that owns a list of SessionFactoryImpl.

    This MultiDbSessionFactory is present in the sessionFactoryMap, with a key
    that denotes it as multi-db.

    The SessionFactoryImpl objects owned by this MultiDbSessionFactory do *not*
    have entries in the map.
 */

class MultiDbSessionFactory implements SessionFactory {

    /* The list */
    private final ArrayList<SessionFactoryImpl> factories =
        new ArrayList<SessionFactoryImpl>();

    /* Map of database name to index number*/
    private final ConcurrentMap<String, Integer> dbNameToNumberMap =
        new ConcurrentHashMap<String, Integer>();

    /** Original spec */
    private final Spec spec;

    /** Original properties */
    private final Map<?,?> props;

    /** SessionFactoryImpl for the default database */
    private final SessionFactoryImpl factory0;

    MultiDbSessionFactory(Spec spec, Map<?,?> props) {
        this.spec = spec;
        this.props = props;
        factory0 = new SessionFactoryImpl(spec, props);
        synchronized(factories) {
            dbNameToNumberMap.put(spec.DATABASE, 0);
            factories.add(factory0);
        }
    }

    public Session getSession() {
        return factory0.getSession();
    }

    int getIndexForDatabase(String databaseName) {
        Integer idx = dbNameToNumberMap.get(databaseName);

        if(idx == null) {
            /* props still contains the name of the default database, but it
               does not matter. The constructed SessionFactoryImpl will use
               the database name in the Spec, not the one in the properties.
            */
            Spec newSpec = new Spec(this.spec, databaseName);
            SessionFactoryImpl newFactory = new SessionFactoryImpl(newSpec, props);
            synchronized(factories) {
                /* Put the factory in the map */
                int databaseNumber = factories.size();
                idx = dbNameToNumberMap.putIfAbsent(databaseName, databaseNumber);
                if(idx != null) {
                    /* We have lost a race */
                    newFactory.close();
                } else {
                    factories.add(newFactory);
                    idx = Integer.valueOf(databaseNumber);
                }
            }
        }
        return idx.intValue();
    }

    public Session getSession(Map properties) {
        String databaseName = PropertyReader.getStringProperty(
            properties, Constants.PROPERTY_CLUSTER_DATABASE);
        if(databaseName == null) return getSession();
        return getSession(databaseName);
    }

    public Session getSession(String databaseName) {
        if(databaseName == null) return getSession();
        int index = getIndexForDatabase(databaseName);
        synchronized(factories) {
            return factories.get(index).getSession();
        }
    }

    public void close() {
        SessionFactoryImpl.removeFactoryFromMap(spec);
        dbNameToNumberMap.clear();

        synchronized(factories) {
            for(SessionFactoryImpl factory : factories)
                factory.close();
            factories.clear();
        }
    }

    public List<Integer> getConnectionPoolSessionCounts() {
        synchronized(factories) {
            // Get the lists from all the factories and sum them
            List<Integer> result = null;
            for(SessionFactory f : factories) {
                if(result == null)
                    result = f.getConnectionPoolSessionCounts();
                else {
                    List<Integer> counts = f.getConnectionPoolSessionCounts();
                    for(int i = 0 ; i < result.size() ; i++)
                        result.set(i, result.get(i) + counts.get(i));
                }
            }
            return result;
        }
    }

    public void reconnect(int timeout) {
        factory0.reconnect(timeout);
    }

    public void reconnect() {
        factory0.reconnect();
    }

    public SessionFactory.State currentState() {
        return factory0.currentState();
    }

    public void setRecvThreadCPUids(short[] cpuids) {
        factory0.setRecvThreadCPUids(cpuids);
    }

    public short[] getRecvThreadCPUids() {
        return factory0.getRecvThreadCPUids();
    }

    public void setRecvThreadActivationThreshold(int threshold) {
        factory0.setRecvThreadActivationThreshold(threshold);
    }

    public int getRecvThreadActivationThreshold() {
        return factory0.getRecvThreadActivationThreshold();
    }
}
