
/*
   Copyright (c) 2010, 2024, Oracle and/or its affiliates.
   Use is subject to license terms.

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

package testsuite.clusterj;

import java.io.IOException;
import java.io.InputStream;

import java.util.ArrayList;
import java.util.List;

import com.mysql.clusterj.Session;

import testsuite.clusterj.model.BlobTypes;

public class BlobInstanceTest extends AbstractClusterJModelTest {

    @Override
    public void localSetUp() {
        createSessionFactory();
        addTearDownClasses(BlobTypes.class);
      }

    static final int ITERATIONS = 500;

    BlobTypes[] instances = null;

    /* The purpose of this test is to induce leaking of DirectByteBuffers
    */

    public void test() {
        instances = new BlobTypes[ITERATIONS];
        create();
        save();
        update();
        failOnError();
        instances = null;
    }

    private void create() {
        for(int i = 0 ; i < ITERATIONS ; i++)
            instances[i] = create(i);
    }

    private void save() {
        for(int i = 0 ; i < ITERATIONS ; i++)
            instances[i] = save(instances[i]);
    }

    private void update() {
        for(int i = 0 ; i < ITERATIONS ; i++)
            update(instances[i]);
    }

    private BlobTypes create(int id) {
        final Session s = sessionFactory.getSession();
        try {
            BlobTypes b = s.newInstance(BlobTypes.class);
            b.setId(id);
            b.setId_null_none(id);
            b.setId_null_hash(id);
            b.setBlobbytes(getBlobBytes(4600 + id));
            return b;
        } finally {
            s.close();
        }
    }

    private BlobTypes save(BlobTypes instance) {
        final Session s = sessionFactory.getSession();
        final int id = instance.getId();
        try {
            BlobTypes b = s.newInstance(BlobTypes.class);
            b.setId(id);
            b.setId_null_none(id+1000);
            b.setId_null_hash(id);
            b.setBlobbytes(instance.getBlobbytes());
            return b;
        } finally {
            s.close();
        }
    }

    private void update(BlobTypes instance) {
        final Session s = sessionFactory.getSession();
        final int id = instance.getId();
        try {
            BlobTypes b = s.newInstance(BlobTypes.class);
            b.setId(id);
            b.setId_null_none(instance.getId_null_none());
            b.setId_null_hash(id+2000);
            b.setBlobbytes(instance.getBlobbytes());
        } finally {
            s.close();
        }
    }


    /** Create a new byte[] of the specified size containing a pattern
     * of bytes in which each byte is the unsigned value of the index
     * modulo 256. This pattern is easy to test.
     * @param size the length of the returned byte[]
     * @return the byte[] filled with the pattern
     */
    protected byte[] getBlobBytes(int size) {
        byte[] result = new byte[size];
        for (int i = 0; i < size; ++i) {
            result[i] = (byte)((i % 256) - 128);
        }
        return result;
    }
}
