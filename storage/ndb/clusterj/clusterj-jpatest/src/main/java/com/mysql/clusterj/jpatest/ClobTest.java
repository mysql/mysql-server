/*
   Copyright (c) 2010, 2012, Oracle and/or its affiliates. All rights reserved.

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

package com.mysql.clusterj.jpatest;

import java.io.IOException;
import java.io.InputStream;
import java.nio.charset.Charset;
import java.nio.charset.CharsetEncoder;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

import com.mysql.clusterj.jpatest.model.ClobTypes;

/** Test clobs
 * Schema:
drop table if exists charsetlatin1;
create table charsetlatin1 (
 id int not null primary key,
 smallcolumn varchar(200),
 mediumcolumn varchar(500),
 largecolumn text(10000)

) ENGINE=ndbcluster DEFAULT CHARSET=latin1;
 
 */
public class ClobTest extends AbstractJPABaseTest {

    /** The size of the clob column is 2 raised to the power i. */
    private static final int NUMBER_TO_INSERT = 16;

    /** The clob instances for testing. */
    protected List<ClobTypes> clobs = new ArrayList<ClobTypes>();

    /** Characters that can be encoded using latin1 character set */
    private char[] chars = getChars(Charset.forName("latin1"));

    /** Subclasses can override this method to get debugging info printed to System.out */
    protected boolean getDebug() {
        return false;
    }

    /** Create a char array with the first 128 encodable characters.
     * 
     * @return the char array
     */
    private char[] getChars(Charset charset) {
        CharsetEncoder encoder = charset.newEncoder();
        char[] result = new char[128];
        char current = (char)0;
        for (int i = 0; i < result.length; ++i) {
            current = nextChar(encoder, current);
            result[i] = current;
        }
        return result;
    }

    /** Get the next char greater than the current char that is encodable
     * by the encoder.
     * @param encoder 
     * 
     */
    char nextChar(CharsetEncoder encoder, char current) {
        current++;
        if (encoder.canEncode(current)) {
            return current;
        } else {
            return nextChar(encoder, current);
        }
    }

    public void test() {
        createClobInstances(NUMBER_TO_INSERT);
        remove();
        insert();
        update();
        failOnError();
    }

    protected void remove() {
        removeAll(ClobTypes.class);
    }

    protected void insert() {
        // insert instances
        tx = em.getTransaction();
        tx.begin();
        
        int count = 0;

        for (int i = 0; i < NUMBER_TO_INSERT; ++i) {
            // must be done with an active transaction
            em.persist(clobs.get(i));
            ++count;
        }
        tx.commit();
    }

    protected void update() {

        tx.begin();

        for (int i = 1; i < NUMBER_TO_INSERT; ++i) {
            // must be done with an active transaction
            ClobTypes e = em.find(ClobTypes.class, i);
            // see if it is the right one
            int actualId = e.getId();
            if (actualId != i) {
                error("Expected ClobTypes.id " + i + " but got " + actualId);
            }
            String string = e.getLarge10000();
            // make sure all fields were fetched properly
            checkString("before update", string, i, false);

            int position = getClobSizeFor(i)/2;
            // only update if the length is correct
            if (string.length() == (position * 2)) {
                StringBuilder sb = new StringBuilder(string);
                // modify the byte in the middle of the blob
                sb.replace(position, position + 1, "!");
                string = sb.toString();
                checkString("after update", string, i, true);
            }
            e.setLarge10000(string);
        }
        tx.commit();
        tx.begin();

        for (int i = 1; i < NUMBER_TO_INSERT; ++i) {
            // must be done with an active transaction
            ClobTypes e = em.find(ClobTypes.class, i);
            // see if it is the right one
            int actualId = e.getId();
            if (actualId != i) {
                error("Expected ClobTypes.id " + i + " but got " + actualId);
            }
            String string = e.getLarge10000();

            // check to see that the blob field has the right data
            checkString("after commit", string, i, true);
        }
        tx.commit();
    }

    protected void createClobInstances(int number) {
        for (int i = 0; i < number; ++i) {
            ClobTypes instance = new ClobTypes();
            instance.setId(i);
            int length = getClobSizeFor(i);
            instance.setLarge10000(getString(length));
//            blob streams are not yet supported
//            instance.setBlobstream(getBlobStream(length));
            clobs.add(instance);
        }
    }

    /** Create a new String of the specified size containing a pattern
     * of characters in which each character is the value of encodable characters
     * at position modulo 128. This pattern is easy to test.
     * @param size the length of the returned string
     * @return the string filled with the pattern
     */
    protected String getString(int size) {
        char[] result = new char[size];
        for (int i = 0; i < size; ++i) {
            result[i] = chars [i%128];
        }
        return new String(result);
    }

    /** Check the string to be sure it matches the pattern in both size
     * and contents.
     * @see getString
     * @param string the string to check
     * @param number the expected length of the string
     * @param updated whether the string is original or updated
     */
    protected void checkString(String where, String string, int number, boolean updated) {
        if (getDebug()) dumpClob(where, string);
        int expectedSize = getClobSizeFor(number);
        int actualSize = string.length();
        if (expectedSize != actualSize) {
            error("In " + where
                    + " wrong size of string; "
                    + "expected: " + expectedSize
                    + " actual: " + actualSize);
        }
        for (int i = 0; i < actualSize; ++i) {
            char expected;
            int position = expectedSize/2;
            if (updated && (i == position)) {
                expected = '!';
            } else {
                expected = chars[i%128];
            }
                char actual = string.charAt(i);
            if (expected != actual) {
                error("In " + where + " for size: " + actualSize
                        + " mismatch in string at position " + i
                        + " expected: " + (int)expected
                        + " actual: " + (int)actual);
            }
        }

    }

    protected InputStream getClobStream(final int i) {
        return new InputStream() {
            int size = i;
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

    protected void dumpClob(String where, String string) {
        System.out.println("In " + where + " dumpClob of size: " + string.length() + " " + Arrays.toString(string.getBytes()));
    }

    protected int getClobSizeFor(int i) {
        int length = (int) Math.pow(2, i);
        return length;
    }

}
