/*
 Copyright (C) 2009 Sun Microsystems, Inc.
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
/*
 * A.java
 */

package myjapi;

// ---------------------------------------------------------------------------
// generatable, user-API-dependent Java wrapper class
// ---------------------------------------------------------------------------

public class A extends jtie.Wrapper {

    // with default constructor, cdelegate needs to be written from JNI
    //protected A() {
    //    System.out.println("<-> myjapi.A()");
    //};

    // this c'tor may me protected, for access from JNI is still possible
    protected A(long cdelegate) {
        super(cdelegate);
        //System.out.println("<-> myjapi.A(" + Long.toHexString(cdelegate) + ")");
    };

    // constructor wrapper
    static public native A create();

    // destructor wrapper
    static public native void delete(A a);

    // static method
    static public native int f0s();

    // non-virtual method
    static public native int f0n(A p0);

    // virtual method
    public native int f0v();

    // factory for Bs
    public native B0 getB0();

    // factory for Bs
    public native B1 getB1();

    // returns an A by ptr
    static public native A return_ptr();

    // returns an A by ptr
    static public native A return_null_ptr();

    // returns an A by ref
    static public native A return_ref();

    // always supposed to raise an exception
    static public native A return_null_ref();

    // takes an A by ptr
    static public native void take_ptr(A p0);

    // takes an A by ptr
    static public native void take_null_ptr(A p0);

    // takes an A by ref
    static public native void take_ref(A p0);

    // never supposed to abort but raise an exception when called with null
    static public native void take_null_ref(A p0);

    // prints an A
    static public native void print(A p0);

    // ----------------------------------------------------------------------

    static public native void h0();

    static public native void h1(byte p0);

    static public native void h2(byte p0, short p1);

    static public native void h3(byte p0, short p1, int p2);

    static public native int h0r();

    static public native int h1r(byte p0);

    static public native int h2r(byte p0, short p1);

    static public native int h3r(byte p0, short p1, int p2);

    static public native void g0c(A obj);

    static public native void g1c(A obj, byte p0);

    static public native void g2c(A obj, byte p0, short p1);

    static public native void g3c(A obj, byte p0, short p1, int p2);

    static public native void g0(A obj);

    static public native void g1(A obj, byte p0);

    static public native void g2(A obj, byte p0, short p1);

    static public native void g3(A obj, byte p0, short p1, int p2);

    static public native int g0rc(A obj);

    static public native int g1rc(A obj, byte p0);

    static public native int g2rc(A obj, byte p0, short p1);

    static public native int g3rc(A obj, byte p0, short p1, int p2);

    static public native int g0r(A obj);

    static public native int g1r(A obj, byte p0);

    static public native int g2r(A obj, byte p0, short p1);

    static public native int g3r(A obj, byte p0, short p1, int p2);
}
