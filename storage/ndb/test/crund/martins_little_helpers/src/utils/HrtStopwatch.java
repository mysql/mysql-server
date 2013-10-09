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
