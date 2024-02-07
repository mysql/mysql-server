/*
 Copyright (c) 2010, 2024, Oracle and/or its affiliates.

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

/*
 * HrtStopwatch.java
 */

package utils;

/**
 * A Java High-Resolution Time Stopwatch Utility.
 */
public class HrtStopwatch {

    static public native void init(int cap);

    static public native void close();

    static public native int top();

    static public native int capacity();

    static public native int pushmark();

    static public native void popmark();

    static public native double rtmicros(int y, int x);

    static public native double ctmicros(int y, int x);

    static public native void clear();
}
