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
 * CI.java
 */

package myjapi;

import com.mysql.jtie.Wrapper;
import com.mysql.jtie.ArrayWrapper;

public class CI {
    static public interface C0C
    {
        long id();
        void check(long id);
        void print();
        C0C deliver_C0Cp();
        C0C deliver_C0Cr();
        void take_C0Cp(C0C cp);
        void take_C0Cr(C0C cp);
    }

    static public class C0 extends Wrapper implements C0C
    {
        protected C0() {
            //System.out.println("<-> myjapi.C0()");
        }

        static public native C0C cc();
        static public native C0 c();

        static public native C0 create();
        static public native void delete(C0 c0);

        static public native C0Array pass(C0Array c0a);
        static public native C0CArray pass(C0CArray c0a);
        static public native long hash(C0CArray c0a, int n);

        public final native long id();
        public final native void check(long id);
        public final native void print();
        public final native C0C deliver_C0Cp();
        public final native C0C deliver_C0Cr();
        public final native void take_C0Cp(C0C cp);
        public final native void take_C0Cr(C0C cp);
        public final native C0 deliver_C0p();
        public final native C0 deliver_C0r();
        public final native void take_C0p(C0 cp);
        public final native void take_C0r(C0 cp);
    }

    static public interface C1C extends C0C
    {
        C1C deliver_C1Cp();
        C1C deliver_C1Cr();
        void take_C1Cp(C1C cp);
        void take_C1Cr(C1C cp);
    }

    static public class C1 extends C0 implements C1C
    {
        protected C1() {
            //System.out.println("<-> myjapi.C1()");
        }

        static public native C1C cc();
        static public native C1 c();

        static public native C1 create();
        static public native void delete(C1 c1);

        static public native C1Array pass(C1Array c1a);
        static public native C1CArray pass(C1CArray c1a);
        static public native long hash(C1CArray c1a, int n);

        public final native C1C deliver_C1Cp();
        public final native C1C deliver_C1Cr();
        public final native void take_C1Cp(C1C cp);
        public final native void take_C1Cr(C1C cp);
        public final native C1 deliver_C1p();
        public final native C1 deliver_C1r();
        public final native void take_C1p(C1 cp);
        public final native void take_C1r(C1 cp);
    }

    static public interface C0CArray
        extends ArrayWrapper< C0C >
    {
        // redundant to declare:
        //C0C at(int i);
    }

    static public final class C0Array
        extends Wrapper implements C0CArray
        // Java: method cannot be inherited with different arguments/results
        //implements C0CArray, ArrayWrapper< C0 >
    {
        static public native C0Array create(int length);
        static public native void delete(C0Array e);
        public native C0 at(int i);
    }

    static public interface C1CArray
        extends ArrayWrapper< C1C >
        // C++: does not support covariant object arrays (only pointers)
        //extends C0CArray
    {
        // if extended C0CArray, would have to declare; otherwise, redundant:
        //C1C at(int i);
    }

    static public final class C1Array
        extends Wrapper implements C1CArray
        // C++: does not support covariant object arrays (only pointers)
        //extends C0Array
        // Java: method cannot be inherited with different arguments/results
        //implements C1CArray, ArrayWrapper< C1 >
    {
        static public native C1Array create(int length);
        static public native void delete(C1Array e);
        public native C1 at(int i);
    }
}
