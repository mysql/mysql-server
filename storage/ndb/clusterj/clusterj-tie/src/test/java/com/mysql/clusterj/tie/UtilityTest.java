/*
 *  Copyright (c) 2010, 2024, Oracle and/or its affiliates.
 *  Use is subject to license terms.
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

package com.mysql.clusterj.tie;

import testsuite.clusterj.AbstractClusterJTest;


/**
 * Tests utility methods.
 */
public class UtilityTest extends AbstractClusterJTest{

    public boolean getDebug() {
        return false;
    }

    public void test() {
        swapShort();
        swapInt();
        swapLong();
        failOnError();
    }

    private void swapShort() {
        short value = (short)0x9876;
        short expected = (short)0x7698;
        short actual = Utility.swap(value);
        if (getDebug()) System.out.println("swap(short) value: " + Integer.toHexString(0xffff & value) + " expected: " + Integer.toHexString(expected) + " actual: " + Integer.toHexString(actual));
        errorIfNotEqual("swap(short) bad value", expected, actual);
    }

    private void swapInt() {
        int value = 0x98765432;
        int expected = 0x32547698;
        int actual = Utility.swap(value);
        if (getDebug()) System.out.println("swap(int) value: " + Integer.toHexString(value) + " expected: " + Integer.toHexString(expected) + " actual: " + Integer.toHexString(actual));
        errorIfNotEqual("swap(int) bad value", expected, actual);
    }

    private void swapLong() {
        long value =    Long.parseLong("01456789abcdef23", 16);
        long expected = Long.parseLong("23efcdab89674501", 16);
        long actual = Utility.swap(value);
        if (getDebug()) System.out.println("swap(long) value: " + Long.toHexString(value) + " expected: " + Long.toHexString(expected) + " actual: " + Long.toHexString(actual));
        errorIfNotEqual("swap(long) bad value", expected, actual);
    }
}
