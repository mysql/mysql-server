/*
 *  Copyright (c) 2024, 2026, Oracle and/or its affiliates.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License, version 2.0,
 *  as published by the Free Software Foundation.
 *
 *  This program is designed to work with certain software (including
 *  but not limited to OpenSSL) that is licensed under separate terms,
 *  as designated in a particular file or component or in included license
 *  documentation.  The authors of MySQL hereby grant you an additional
 *  permission to link the program and your derivative works with the
 *  separately licensed software that they have either included with
 *  the program or referenced in the documentation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License, version 2.0, for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

package testsuite.clusterj;

import com.mysql.clusterj.ClusterJFatalUserException;
import com.mysql.clusterj.ClusterJHelper;
import com.mysql.clusterj.Constants;
import com.mysql.clusterj.Session;
import com.mysql.clusterj.SessionFactory;

import java.util.Properties;

import testsuite.clusterj.model.BlobTypes;

public class ByteBufferPoolTest extends AbstractClusterJTest {

    @Override
    protected void localSetUp() {
        createSessionFactory();
        createSession();
        addTearDownClasses(BlobTypes.class);
    }

    public static void setPoolSizes(Properties p, String spec) {
        p.put(Constants.PROPERTY_CLUSTER_BYTE_BUFFER_POOL_SIZES, spec);
    }

    public static void printSizes(Properties p, String testName) {
        logger.info(() -> testName + " Sizes: " +
                          p.get("com.mysql.clusterj.byte.buffer.pool.sizes"));
    }

    protected void storeBlob(Session session, int id, int size) {
        byte[] content = BlobTest.getBlobBytes(size);
        BlobTypes instance = session.newInstance(BlobTypes.class);
        instance.setId(id);
        instance.setBlobbytes(content);
        session.persist(instance);
    }

    protected void storeOneBlob(SessionFactory factory, int id, int size) {
        Session session = factory.getSession();
        storeBlob(session, id, size);
        session.close();
    }


    public void testDefaultPool() {
        Properties properties = props;
        printSizes(properties, "testDefaultPool");
        SessionFactory factory = ClusterJHelper.getSessionFactory(properties);
        storeOneBlob(factory, 1, 10000);
    }

    public void testSmallPool() {
        Properties properties = new Properties();
        properties.putAll(props);
        setPoolSizes(properties, "512, 51200");
        printSizes(properties, "testSmallPool");
        SessionFactory factory = ClusterJHelper.getSessionFactory(properties);
        logger.warn(" >> Expect WARNING ... requested: 65,000; maximum: 51,200. ");
        storeOneBlob(factory, 2, 65000);
        factory.close();
    }
}

