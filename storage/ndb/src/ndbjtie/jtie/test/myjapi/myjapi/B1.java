/*
 Copyright (c) 2010, 2022, Oracle and/or its affiliates.

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
/*
 * B1.java
 */

package myjapi;

public class B1 extends B0 {
    // this c'tor may me protected, for access from JNI is still possible
    // with default constructor, cdelegate needs to be written from JNI
    protected B1() {
        //System.out.println("<-> myjapi.B1()");
    };

    // static method
    static public native int f0s();

    // non-virtual method
    static public native int f0n(B1 obj);

    // virtual method
    public native int f0v();

    // static const field accessor
    static public native int d0sc();

    // static field accessor
    static public native int d0s();

    // static field mutator
    static public native void d0s(int d);

    // instance const field accessor
    static public native int d0c(B1 obj);

    // instance field accessor
    static public native int d0(B1 obj);

    // instance field mutator
    static public native void d0(B1 obj, int d);
}
