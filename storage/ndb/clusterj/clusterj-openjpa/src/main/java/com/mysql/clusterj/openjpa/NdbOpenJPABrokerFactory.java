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

package com.mysql.clusterj.openjpa;

import com.mysql.clusterj.core.util.I18NHelper;
import com.mysql.clusterj.core.util.Logger;
import com.mysql.clusterj.core.util.LoggerFactoryService;

import java.util.Map;

import org.apache.openjpa.jdbc.kernel.JDBCBrokerFactory;

import org.apache.openjpa.kernel.Bootstrap;
import org.apache.openjpa.kernel.StoreManager;

import org.apache.openjpa.lib.conf.ConfigurationProvider;

/**
 * BrokerFactory for use with the Ndb runtime.
 *
 */
@SuppressWarnings("serial")
public class NdbOpenJPABrokerFactory extends JDBCBrokerFactory {

    /** My message translator */
    static final I18NHelper local = I18NHelper.getInstance(NdbOpenJPABrokerFactory.class);

    /** My logger */
    static final Logger logger = LoggerFactoryService.getFactory().getInstance(NdbOpenJPABrokerFactory.class);

    /**
     * Factory method for constructing a factory from properties. Invoked from
     * {@link Bootstrap#newBrokerFactory}.
     */
    public static NdbOpenJPABrokerFactory newInstance(ConfigurationProvider cp) {
        NdbOpenJPAConfigurationImpl conf = new NdbOpenJPAConfigurationImpl();
        cp.setInto(conf);
        return new NdbOpenJPABrokerFactory(conf);
    }

    /**
     * Factory method for obtaining a possibly-pooled factory from properties.
     * Invoked from {@link Bootstrap#getBrokerFactory}.
     */
    public static NdbOpenJPABrokerFactory getInstance(ConfigurationProvider cp) {
        Map<String, Object> props = cp.getProperties();
        Object key = toPoolKey(props);
        NdbOpenJPABrokerFactory factory = (NdbOpenJPABrokerFactory)
            getPooledFactoryForKey(key);
        if (factory != null)
            return factory;

        factory = newInstance(cp);
        pool(key, factory);
        return factory;
    }

    /**
     * Construct the factory with the given option settings; however, the
     * factory construction methods are recommended.
     */
    public NdbOpenJPABrokerFactory(NdbOpenJPAConfiguration conf) {
        super(conf);
        if (logger.isInfoEnabled()) {
            StringBuffer buffer = new StringBuffer();
            buffer.append("connectString: " + conf.getConnectString());
            buffer.append("; connectDelay: " + conf.getConnectDelay());
            buffer.append("; connectVerbose: " + conf.getConnectVerbose());
            buffer.append("; connectTimeoutBefore: " + conf.getConnectTimeoutBefore());
            buffer.append("; connectTimeoutAfter: " + conf.getConnectTimeoutAfter());
            buffer.append("; maxTransactions: " + conf.getMaxTransactions());
            buffer.append("; default database: " + conf.getDatabase());
            logger.info(buffer.toString());
        }
    }

    @Override
    protected StoreManager newStoreManager() {
        return new NdbOpenJPAStoreManager();
    }

}
