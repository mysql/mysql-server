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
 * CI.java
 */

package myjapi;

import com.mysql.jtie.Wrapper;

public class CI {
    static public interface C0C {

        void print();

        C0C deliver_C0Cp();

        C0C deliver_C0Cr();

        void take_C0Cp(C0C cp);

        void take_C0Cr(C0C cp);
    }

    static public class C0 extends Wrapper implements C0C {

        protected C0() {
            System.out.println("<-> myjapi.C0()");
        }

        static public native C0C cc();

        static public native C0 c();

        public native void print();

        public native C0C deliver_C0Cp();

        public native C0C deliver_C0Cr();

        public native void take_C0Cp(C0C cp);

        public native void take_C0Cr(C0C cp);

        public native C0 deliver_C0p();

        public native C0 deliver_C0r();

        public native void take_C0p(C0 cp);

        public native void take_C0r(C0 cp);

        // map enums as ints

        static public final int C0E0 = 0;

        static public final int C0E1 = 1;

        static public native int/*_C0E_*/ deliver_C0E1();

        static public native void take_C0E1(int/*_C0E_*/ e);

        static public native int/*_const C0E_*/ deliver_C0E1c();

        static public native void take_C0E1c(int/*_const C0E_*/ e);
    }

    static public interface C1C extends C0C {

        C1C deliver_C1Cp();

        C1C deliver_C1Cr();

        void take_C1Cp(C1C cp);

        void take_C1Cr(C1C cp);
    }

    static public class C1 extends C0 implements C1C {

        protected C1() {
            System.out.println("<-> myjapi.C1()");
        }

        static public native C1C cc();

        static public native C1 c();

        public native C1C deliver_C1Cp();

        public native C1C deliver_C1Cr();

        public native void take_C1Cp(C1C cp);

        public native void take_C1Cr(C1C cp);

        public native C1 deliver_C1p();

        public native C1 deliver_C1r();

        public native void take_C1p(C1 cp);

        public native void take_C1r(C1 cp);
    }
}
