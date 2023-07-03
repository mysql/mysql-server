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
 * A.java
 */

package myjapi;

public class A extends com.mysql.jtie.Wrapper {
    // this c'tor may me protected, for access from JNI is still possible
    // with default constructor, cdelegate needs to be written from JNI
    protected A() {
        //System.out.println("<-> myjapi.A()");
    };

    // constructor wrapper (mapped by reference)
    static public native A create_r();

    // constructor wrapper (mapped by reference)
    static public native A create_r(int p0);

    // constructor wrapper (mapped by pointer)
    static public native A create_p();

    // constructor wrapper (mapped by pointer)
    static public native A create_p(int p0);

    // destructor wrapper (mapped by reference)
    static public native void delete_r(A a);

    // destructor wrapper (mapped by pointer)
    static public native void delete_p(A a);

    // static method
    static public native int f0s();

    // non-virtual method
    static public native int f0n(A p0);

    // virtual method
    public native int f0v();

    // creates a B0
    public native B0 newB0();

    // creates a B1
    public native B1 newB1();

    // deletes a B0
    public native void del(B0 b);

    // deletes a B1
    public native void del(B1 b);

    // returns an A
    static public native A deliver_ptr();

    // returns NULL
    static public native A deliver_null_ptr();

    // returns an A
    static public native A deliver_ref();

    // always supposed to raise an exception
    static public native A deliver_null_ref();

    // requires the A returned by deliver_ptr()
    static public native void take_ptr(A p0);

    // requires NULL
    static public native void take_null_ptr(A p0);

    // requires the A returned by deliver_ref()
    static public native void take_ref(A p0);

    // never supposed to abort but raise an exception when called with null
    static public native void take_null_ref(A p0);

    // prints an A
    static public native void print(A p0);

    // ----------------------------------------------------------------------

    // static const field accessor
    static public final native int d0sc();

    // static field accessor
    static public final native int d0s();

    // static field mutator
    static public final native void d0s(int d);

    // instance const field accessor
    public final native int d0c();

    // instance field accessor
    public final native int d0();

    // instance field mutator
    public final native void d0(int d);

    // ----------------------------------------------------------------------

    public native final void g0c();

    public native final void g1c(byte p0);

    public native final void g2c(byte p0, short p1);

    public native final void g3c(byte p0, short p1, int p2);

    public native final void g0();

    public native final void g1(byte p0);

    public native final void g2(byte p0, short p1);

    public native final void g3(byte p0, short p1, int p2);

    public native final int g0rc();

    public native final int g1rc(byte p0);

    public native final int g2rc(byte p0, short p1);

    public native final int g3rc(byte p0, short p1, int p2);

    public native final int g0r();

    public native final int g1r(byte p0);

    public native final int g2r(byte p0, short p1);

    public native final int g3r(byte p0, short p1, int p2);

    static public native void h0();

    static public native void h1(byte p0);

    static public native void h2(byte p0, short p1);

    static public native void h3(byte p0, short p1, int p2);

    static public native int h0r();

    static public native int h1r(byte p0);

    static public native int h2r(byte p0, short p1);

    static public native int h3r(byte p0, short p1, int p2);
}
