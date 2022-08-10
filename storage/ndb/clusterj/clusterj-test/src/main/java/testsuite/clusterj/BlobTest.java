/*
   Copyright (c) 2010, 2022, Oracle and/or its affiliates.
   Use is subject to license terms.

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

package testsuite.clusterj;

import java.io.IOException;
import java.io.InputStream;

import java.util.ArrayList;
import java.util.List;

import testsuite.clusterj.model.BlobTypes;

public class BlobTest extends AbstractClusterJModelTest {

    /** The size of the blob column is 2 raised to the power i. */
    private static final int NUMBER_TO_INSERT = 16;

    /** The blob instances for testing. */
    protected List<BlobTypes> blobs = new ArrayList<BlobTypes>();

    @Override
    public void localSetUp() {
        createSessionFactory();
        session = sessionFactory.getSession();
        createBlobInstances(NUMBER_TO_INSERT);
        tx = session.currentTransaction();
        tx.begin();
        session.deletePersistentAll(BlobTypes.class);
        tx.commit();
        // the following tests the delete functionality
        addTearDownClasses(BlobTypes.class);
    }

    public void test() {
        insert();
        update();
        failOnError();
    }

    protected void insert() {
        // insert instances
        tx = session.currentTransaction();
        tx.begin();
        
        int count = 0;

        for (int i = 0; i < NUMBER_TO_INSERT; ++i) {
            // must be done with an active transaction
            session.makePersistent(blobs.get(i));
            ++count;
        }
        tx.commit();
    }

    protected void update() {

        tx.begin();

        for (int i = 1; i < NUMBER_TO_INSERT; ++i) {
            // must be done with an active transaction
            BlobTypes e = session.find(BlobTypes.class, i);
            // see if it is the right one
            int actualId = e.getId();
            if (actualId != i) {
                error("Expected BlobTypes.id " + i + " but got " + actualId);
            }
            byte[] bytes = e.getBlobbytes();
            if (bytes == null) {
                error("Unexpected blob bytes null for id " + i);
            } else {
                // make sure all fields were fetched properly
                checkBlobBytes("before update", bytes, i, false);

                int position = getBlobSizeFor(i)/2;
                // only update if the length is correct
                if (bytes.length == (position * 2)) {
                    // modify the byte in the middle of the blob
                    bytes[position] = (byte)(position % 128);
                    checkBlobBytes("after update", bytes, i, true);

                    // mark the field as modified so it will be flushed
                    session.markModified(e, "blobbytes");

                    // update the modified instance
                    session.updatePersistent(e);
                }
            }
        }
        tx.commit();
        tx.begin();

        for (int i = 1; i < NUMBER_TO_INSERT; ++i) {
            // must be done with an active transaction
            BlobTypes e = session.find(BlobTypes.class, i);
            // see if it is the right one
            int actualId = e.getId();
            if (actualId != i) {
                error("Expected BlobTypes.id " + i + " but got " + actualId);
            }
            byte[] bytes = e.getBlobbytes();
            if (bytes == null) {
                error("Unexpected blob bytes null for id " + i);
            } else {

            // check to see that the blob field has the right data
            checkBlobBytes("after commit", e.getBlobbytes(), i, true);
            }
        }
        tx.commit();
    }

    protected void createBlobInstances(int number) {
        for (int i = 0; i < number; ++i) {
            BlobTypes instance = session.newInstance(BlobTypes.class);
            instance.setId(i);
            int length = getBlobSizeFor(i);
            instance.setBlobbytes(getBlobBytes(length));
//            blob streams are not yet supported
//            instance.setBlobstream(getBlobStream(length));
            blobs.add(instance);
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

    /** Check the byte[] to be sure it matches the pattern in both size
     * and contents.
     * @see getBlobBytes
     * @param bytes the byte[] to check
     * @param number the expected length of the byte[]
     */
    protected void checkBlobBytes(String where, byte[] bytes, int number, boolean updated) {
        // debugging statement; comment out once test passes
        dumpBlob(where, bytes);
        int expectedSize = getBlobSizeFor(number);
        int actualSize = bytes.length;
        if (expectedSize != actualSize) {
            error("In " + where
                    + " wrong size of byte[]; "
                    + "expected: " + expectedSize
                    + " actual: " + actualSize);
        }
        for (int i = 0; i < actualSize; ++i) {
            byte expected;
            int position = expectedSize/2;
            if (updated && (i == position)) {
                expected = (byte)(position % 128);
            } else {
                expected = (byte)((i % 256) - 128);
            }
                byte actual = bytes[i];
            if (expected != actual) {
                error("In " + where + " for size: " + actualSize
                        + " mismatch in byte[] at position " + i
                        + " expected: " + expected
                        + " actual: " + actual);
            }
        }

    }

    protected InputStream getBlobStream(final int i) {
        return new InputStream() {
            int counter = 0;
            @Override
            public int read() throws IOException {
                if (counter >= i) {
                    return -1;
                } else {
                    return counter++ %256;
                }
            }
        };
    }

    protected void dumpBlob(String where, byte[] blob) {
//        System.out.println("In " + where + " dumpBlob of size: " + blob.length);
//        for (byte b: blob) {
//            System.out.print("[" + b + "]");
//        }
//        System.out.println();
    }

    protected int getBlobSizeFor(int i) {
        int length = (int) Math.pow(2, i);
        return length;
    }

}
