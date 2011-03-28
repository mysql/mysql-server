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
 * MyJapiTest.java
 */

package test;

import java.io.PrintWriter;

import java.math.BigInteger;
//import java.math.BigDecimal;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.CharBuffer;
import java.nio.ShortBuffer;
import java.nio.IntBuffer;
import java.nio.LongBuffer;
import java.nio.FloatBuffer;
import java.nio.DoubleBuffer;

import myjapi.MyJapi;
import myjapi.MyJapiCtypes;
import myjapi.A;
import myjapi.B0;
import myjapi.B1;

public class MyJapiTest {

    static protected final PrintWriter out = new PrintWriter(System.out, true);

    static protected final PrintWriter err = new PrintWriter(System.err, true);

    /**
     * Loads a dynamically linked system library and reports any failures.
     */
    static protected void loadSystemLibrary(String name) {
        out.print("loading libary ...");
        out.flush();
        try {
            System.loadLibrary(name);
        } catch (UnsatisfiedLinkError e) {
            String path;
            try {
                path = System.getProperty("java.library.path");
            } catch (Exception ex) {
                path = "<exception caught: " + ex.getMessage() + ">";
            }
            err.println("failed loading library '"
                        + name + "'; java.library.path='" + path + "'");
            throw e;
        } catch (SecurityException e) {
            err.println("failed loading library '"
                        + name + "'; caught exception: " + e);
            throw e;
        }
        out.println("          [" + name + "]");
    }

/*    
    static public abstract class Run
    {
        private String name;

        public Run(String name) {
            this.name = name;
        }

        public abstract T call();
        
        public abstract T call();
        
        public void test(T e) {
            out.println();
            out.println("calling " + name + "() ...");
            T r = call();
            if (e.equals(r))
                throw new RuntimeException("e = " + e + ", r = " + r);
        }
        
        public void run(int n) {
            if 
            for (int i = 1; i < 3; i++) {
                test(i);
            }
        }
    }
*/

    static public void test0() {
        out.println("--> MyJapiTest.test0()");

        out.println();
        out.println("testing basic MyJapi function: f0() ...");
        out.println();

        out.println("\ncalling f0()");
        MyJapi.f0();

        out.println();
        out.println("testing MyJapiCtypes functions: fxx(0) ...");
        out.println();

        for (int i = 0; i < 2; i++) {
            out.println("\ncalling f11()");
            final boolean nf11 = MyJapiCtypes.f11(false);
            assert(nf11 == false);
            out.println("\ncalling f12()");
            final byte nf12 = MyJapiCtypes.f12((byte)0);
            assert(nf12 == (byte)0);
            out.println("\ncalling f13()");
            final byte nf13 = MyJapiCtypes.f13((byte)0);
            assert(nf13 == (byte)0);
            out.println("\ncalling f14()");
            final byte nf14 = MyJapiCtypes.f14((byte)0);
            assert(nf14 == (byte)0);
            out.println("\ncalling f15()");
            final short nf15 = MyJapiCtypes.f15((short)0);
            assert(nf15 == (short)0);
            out.println("\ncalling f16()");
            final short nf16 = MyJapiCtypes.f16((short)0);
            assert(nf16 == (short)0);
            out.println("\ncalling f17()");
            final int nf17 = MyJapiCtypes.f17((int)0);
            assert(nf17 == (int)0);
            out.println("\ncalling f18()");
            final int nf18 = MyJapiCtypes.f18((int)0);
            assert(nf18 == (int)0);
            out.println("\ncalling f19()");
            final int nf19 = MyJapiCtypes.f19((int)0);
            assert(nf19 == (int)0);
            out.println("\ncalling f20()");
            final int nf20 = MyJapiCtypes.f20((int)0);
            assert(nf20 == (int)0);
            out.println("\ncalling f21()");
            final long nf21 = MyJapiCtypes.f21((long)0);
            assert(nf21 == (long)0);
            out.println("\ncalling f22()");
            final long nf22 = MyJapiCtypes.f22((long)0);
            assert(nf22 == (long)0);
            out.println("\ncalling f23()");
            final float nf23 = MyJapiCtypes.f23((float)0);
            assert(nf23 == (float)0);
            out.println("\ncalling f24()");
            final double nf24 = MyJapiCtypes.f24((double)0);
            assert(nf24 == (double)0);
            out.println("\ncalling f25()");
            final double nf25 = MyJapiCtypes.f25((double)0);
            assert(nf25 == (double)0);

            out.println("\ncalling f31()");
            final boolean nf31 = MyJapiCtypes.f31(false);
            assert(nf31 == false);
            out.println("\ncalling f32()");
            final byte nf32 = MyJapiCtypes.f32((byte)0);
            assert(nf32 == (byte)0);
            out.println("\ncalling f33()");
            final byte nf33 = MyJapiCtypes.f33((byte)0);
            assert(nf33 == (byte)0);
            out.println("\ncalling f34()");
            final byte nf34 = MyJapiCtypes.f34((byte)0);
            assert(nf34 == (byte)0);
            out.println("\ncalling f35()");
            final short nf35 = MyJapiCtypes.f35((short)0);
            assert(nf35 == (short)0);
            out.println("\ncalling f36()");
            final short nf36 = MyJapiCtypes.f36((short)0);
            assert(nf36 == (short)0);
            out.println("\ncalling f37()");
            final int nf37 = MyJapiCtypes.f37((int)0);
            assert(nf37 == (int)0);
            out.println("\ncalling f38()");
            final int nf38 = MyJapiCtypes.f38((int)0);
            assert(nf38 == (int)0);
            out.println("\ncalling f39()");
            final int nf39 = MyJapiCtypes.f39((int)0);
            assert(nf39 == (int)0);
            out.println("\ncalling f40()");
            final int nf40 = MyJapiCtypes.f40((int)0);
            assert(nf40 == (int)0);
            out.println("\ncalling f41()");
            final long nf41 = MyJapiCtypes.f41((long)0);
            assert(nf41 == (long)0);
            out.println("\ncalling f42()");
            final long nf42 = MyJapiCtypes.f42((long)0);
            assert(nf42 == (long)0);
            out.println("\ncalling f43()");
            final float nf43 = MyJapiCtypes.f43((float)0);
            assert(nf43 == (float)0);
            out.println("\ncalling f44()");
            final double nf44 = MyJapiCtypes.f44((double)0);
            assert(nf44 == (double)0);
            out.println("\ncalling f45()");
            final double nf45 = MyJapiCtypes.f45((double)0);
            assert(nf45 == (double)0);
        }

        out.println();
        out.println("<-- MyJapiTest.test0()");
    }

    static public void test1() {
        out.println("--> MyJapiTest.test1()");

        out.println();
        out.println("testing basic MyJapi functions: f1xx(f0xx()) ...");
        out.println();

        out.println("\ncalling f0()");
        MyJapi.f0();

        for (int i = 0; i < 2; i++) {
            out.println("\ncalling f111(f011())");
            MyJapi.f111(MyJapi.f011());
            out.println("\ncalling f112(f012())");
            MyJapi.f112(MyJapi.f012());
            out.println("\ncalling f113(f013())");
            MyJapi.f113(MyJapi.f013());
            out.println("\ncalling f114(f014())");
            MyJapi.f114(MyJapi.f014());
            out.println("\ncalling f115(f015())");
            MyJapi.f115(MyJapi.f015());
            out.println("\ncalling f116(f016())");
            MyJapi.f116(MyJapi.f016());
            out.println("\ncalling f117(f017())");
            MyJapi.f117(MyJapi.f017());
            out.println("\ncalling f118(f018())");
            MyJapi.f118(MyJapi.f018());
            out.println("\ncalling f121(f021())");
            MyJapi.f121(MyJapi.f021());
            out.println("\ncalling f122(f022())");
            MyJapi.f122(MyJapi.f022());
            out.println("\ncalling f123(f023())");
            MyJapi.f123(MyJapi.f023());
            out.println("\ncalling f124(f024())");
            MyJapi.f124(MyJapi.f024());

            out.println("\ncalling f131(f031())");
            MyJapi.f131(MyJapi.f031());
            out.println("\ncalling f132(f032())");
            MyJapi.f132(MyJapi.f032());
            out.println("\ncalling f133(f033())");
            MyJapi.f133(MyJapi.f033());
            out.println("\ncalling f134(f034())");
            MyJapi.f134(MyJapi.f034());
            out.println("\ncalling f135(f035())");
            MyJapi.f135(MyJapi.f035());
            out.println("\ncalling f136(f036())");
            MyJapi.f136(MyJapi.f036());
            out.println("\ncalling f137(f037())");
            MyJapi.f137(MyJapi.f037());
            out.println("\ncalling f138(f038())");
            MyJapi.f138(MyJapi.f038());
            out.println("\ncalling f141(f041())");
            MyJapi.f141(MyJapi.f041());
            out.println("\ncalling f142(f042())");
            MyJapi.f142(MyJapi.f042());
            out.println("\ncalling f143(f043())");
            MyJapi.f143(MyJapi.f043());
            out.println("\ncalling f144(f044())");
            MyJapi.f144(MyJapi.f044());
        }

        out.println();
        out.println("<-- MyJapiTest.test1()");
    }
    
    static public void test2() {
        out.println("--> MyJapiTest.test2()");

        out.println();
        out.println("testing basic MyJapi functions: f3xxbb(f2xxbb()) ...");
        out.println();

        // XXX todo: test
        // java/lang/IllegalArgumentException
        // java/nio/ReadOnlyBufferException

        if (false) {
            out.println("\ncalling f237bb()");
            ByteBuffer bb = MyJapi.f237bb();
            out.println("bb = " + bb);
            bb.order(ByteOrder.nativeOrder()); // initial order is big endian!
            IntBuffer ib = bb.asIntBuffer();
            out.println("ib = " + ib);
            out.println("ib.get() = " + ib.get());
            out.println("\ncalling f337bb()");
            MyJapi.f337bb(bb);
        }
        if (false) {
            out.println("\ncalling f217()");
            ByteBuffer bb = MyJapi.f217bb();
            out.println("bb = " + bb);
            bb.order(ByteOrder.nativeOrder()); // initial order is big endian!
            IntBuffer ib = bb.asIntBuffer();
            out.println("ib = " + ib);
            out.println("ib.get() = " + ib.get());
            out.println("\ncalling f317bb()");
            MyJapi.f317bb(bb);
        }

        for (int i = 0; i < 2; i++) {
            out.println("\ncalling f311bb(f211bb())");
            final ByteBuffer f211bb = MyJapi.f211bb().order(ByteOrder.nativeOrder());
            final byte nf211bb = f211bb.asReadOnlyBuffer().get();
            MyJapi.f311bb(f211bb);
            assert (nf211bb == f211bb.asReadOnlyBuffer().get());
            MyJapi.f311bb(MyJapi.f211bb());
            assert (nf211bb - 1 == f211bb.asReadOnlyBuffer().get());

            out.println("\ncalling f312bb(f212bb())");
            final ByteBuffer f212bb = MyJapi.f212bb().order(ByteOrder.nativeOrder());
            final byte nf212bb = f212bb.asReadOnlyBuffer().get();
            MyJapi.f312bb(f212bb);
            assert (nf212bb == f212bb.asReadOnlyBuffer().get());
            MyJapi.f312bb(MyJapi.f212bb());
            assert (nf212bb + 1 == f212bb.asReadOnlyBuffer().get());

            out.println("\ncalling f313bb(f213bb())");
            final ByteBuffer f213bb = MyJapi.f213bb().order(ByteOrder.nativeOrder());
            final byte nf213bb = f213bb.asReadOnlyBuffer().get();
            MyJapi.f313bb(f213bb);
            assert (nf213bb == f213bb.asReadOnlyBuffer().get());
            MyJapi.f313bb(MyJapi.f213bb());
            assert (nf213bb + 1 == f213bb.asReadOnlyBuffer().get());

            out.println("\ncalling f314bb(f214bb())");
            final ByteBuffer f214bb = MyJapi.f214bb().order(ByteOrder.nativeOrder());
            final byte nf214bb = f214bb.asReadOnlyBuffer().get();
            MyJapi.f314bb(f214bb);
            assert (nf214bb == f214bb.asReadOnlyBuffer().get());
            MyJapi.f314bb(MyJapi.f214bb());
            assert (nf214bb + 1 == f214bb.asReadOnlyBuffer().get());

            out.println("\ncalling f315bb(f215bb())");
            final ByteBuffer f215bb = MyJapi.f215bb().order(ByteOrder.nativeOrder());
            final short nf215bb = f215bb.asShortBuffer().get();
            MyJapi.f315bb(f215bb);
            assert (nf215bb == f215bb.asShortBuffer().get());
            MyJapi.f315bb(MyJapi.f215bb());
            assert (nf215bb + 1 == f215bb.asShortBuffer().get());

            out.println("\ncalling f316bb(f216bb())");
            final ByteBuffer f216bb = MyJapi.f216bb().order(ByteOrder.nativeOrder());
            final short nf216bb = f216bb.asShortBuffer().get();
            MyJapi.f316bb(f216bb);
            assert (nf216bb == f216bb.asShortBuffer().get());
            MyJapi.f316bb(MyJapi.f216bb());
            assert (nf216bb + 1 == f216bb.asShortBuffer().get());

            out.println("\ncalling f317bb(f217bb())");
            final ByteBuffer f217bb = MyJapi.f217bb().order(ByteOrder.nativeOrder());
            final int nf217bb = f217bb.asIntBuffer().get();
            MyJapi.f317bb(f217bb);
            assert (nf217bb == f217bb.asIntBuffer().get());
            MyJapi.f317bb(MyJapi.f217bb());
            assert (nf217bb + 1 == f217bb.asIntBuffer().get());

            out.println("\ncalling f318bb(f218bb())");
            final ByteBuffer f218bb = MyJapi.f218bb().order(ByteOrder.nativeOrder());
            final int nf218bb = f218bb.asIntBuffer().get();
            MyJapi.f318bb(f218bb);
            assert (nf218bb == f218bb.asIntBuffer().get());
            MyJapi.f318bb(MyJapi.f218bb());
            assert (nf218bb + 1 == f218bb.asIntBuffer().get());

            out.println("\ncalling f321bb(f221bb())");
            final ByteBuffer f221bb = MyJapi.f221bb().order(ByteOrder.nativeOrder());
            final long nf221bb = f221bb.asLongBuffer().get();
            MyJapi.f321bb(f221bb);
            assert (nf221bb == f221bb.asLongBuffer().get());
            MyJapi.f321bb(MyJapi.f221bb());
            assert (nf221bb + 1 == f221bb.asLongBuffer().get());

            out.println("\ncalling f322bb(f222bb())");
            final ByteBuffer f222bb = MyJapi.f222bb().order(ByteOrder.nativeOrder());
            final long nf222bb = f222bb.asLongBuffer().get();
            MyJapi.f322bb(f222bb);
            assert (nf222bb == f222bb.asLongBuffer().get());
            MyJapi.f322bb(MyJapi.f222bb());
            assert (nf222bb + 1 == f222bb.asLongBuffer().get());

            out.println("\ncalling f323bb(f223bb())");
            final ByteBuffer f223bb = MyJapi.f223bb().order(ByteOrder.nativeOrder());
            final float nf223bb = f223bb.asFloatBuffer().get();
            MyJapi.f323bb(f223bb);
            assert (nf223bb == f223bb.asFloatBuffer().get());
            MyJapi.f323bb(MyJapi.f223bb());
            assert (nf223bb + 1 == f223bb.asFloatBuffer().get());

            out.println("\ncalling f324bb(f224bb())");
            final ByteBuffer f224bb = MyJapi.f224bb().order(ByteOrder.nativeOrder());
            final double nf224bb = f224bb.asDoubleBuffer().get();
            MyJapi.f324bb(f224bb);
            assert (nf224bb == f224bb.asDoubleBuffer().get());
            MyJapi.f324bb(MyJapi.f224bb());
            assert (nf224bb + 1 == f224bb.asDoubleBuffer().get());

            out.println("\ncalling f331bb(f231bb())");
            final ByteBuffer f231bb = MyJapi.f231bb().order(ByteOrder.nativeOrder());
            final byte nf231bb = f231bb.asReadOnlyBuffer().get();
            MyJapi.f331bb(f231bb);
            assert (nf231bb - 1 == f231bb.asReadOnlyBuffer().get());

            out.println("\ncalling f332bb(f232bb())");
            final ByteBuffer f232bb = MyJapi.f232bb().order(ByteOrder.nativeOrder());
            final byte nf232bb = f232bb.asReadOnlyBuffer().get();
            MyJapi.f332bb(f232bb);
            assert (nf232bb + 1 == f232bb.asReadOnlyBuffer().get());

            out.println("\ncalling f333bb(f233bb())");
            final ByteBuffer f233bb = MyJapi.f233bb().order(ByteOrder.nativeOrder());
            final byte nf233bb = f233bb.asReadOnlyBuffer().get();
            MyJapi.f333bb(f233bb);
            assert (nf233bb + 1 == f233bb.asReadOnlyBuffer().get());

            out.println("\ncalling f334bb(f234bb())");
            final ByteBuffer f234bb = MyJapi.f234bb().order(ByteOrder.nativeOrder());
            final byte nf234bb = f234bb.asReadOnlyBuffer().get();
            MyJapi.f334bb(f234bb);
            assert (nf234bb + 1 == f234bb.asReadOnlyBuffer().get());

            out.println("\ncalling f335bb(f235bb())");
            final ByteBuffer f235bb = MyJapi.f235bb().order(ByteOrder.nativeOrder());
            final short nf235bb = f235bb.asShortBuffer().get();
            MyJapi.f335bb(f235bb);
            assert (nf235bb + 1 == f235bb.asShortBuffer().get());

            out.println("\ncalling f336bb(f236bb())");
            final ByteBuffer f236bb = MyJapi.f236bb().order(ByteOrder.nativeOrder());
            final short nf236bb = f236bb.asShortBuffer().get();
            MyJapi.f336bb(f236bb);
            assert (nf236bb + 1 == f236bb.asShortBuffer().get());

            out.println("\ncalling f337bb(f237bb())");
            final ByteBuffer f237bb = MyJapi.f237bb().order(ByteOrder.nativeOrder());
            final int nf237bb = f237bb.asIntBuffer().get();
            MyJapi.f337bb(f237bb);
            assert (nf237bb + 1 == f237bb.asIntBuffer().get());

            out.println("\ncalling f338bb(f238bb())");
            final ByteBuffer f238bb = MyJapi.f238bb().order(ByteOrder.nativeOrder());
            final int nf238bb = f238bb.asIntBuffer().get();
            MyJapi.f338bb(f238bb);
            assert (nf238bb + 1 == f238bb.asIntBuffer().get());

            out.println("\ncalling f341bb(f241bb())");
            final ByteBuffer f241bb = MyJapi.f241bb().order(ByteOrder.nativeOrder());
            final long nf241bb = f241bb.asLongBuffer().get();
            MyJapi.f341bb(f241bb);
            assert (nf241bb + 1 == f241bb.asLongBuffer().get());

            out.println("\ncalling f342bb(f242bb())");
            final ByteBuffer f242bb = MyJapi.f242bb().order(ByteOrder.nativeOrder());
            final long nf242bb = f242bb.asLongBuffer().get();
            MyJapi.f342bb(f242bb);
            assert (nf242bb + 1 == f242bb.asLongBuffer().get());

            out.println("\ncalling f343bb(f243bb())");
            final ByteBuffer f243bb = MyJapi.f243bb().order(ByteOrder.nativeOrder());
            final float nf243bb = f243bb.asFloatBuffer().get();
            MyJapi.f343bb(f243bb);
            assert (nf243bb + 1 == f243bb.asFloatBuffer().get());

            out.println("\ncalling f344bb(f244bb())");
            final ByteBuffer f244bb = MyJapi.f244bb().order(ByteOrder.nativeOrder());
            final double nf244bb = f244bb.asDoubleBuffer().get();
            MyJapi.f344bb(f244bb);
            assert (nf244bb + 1 == f244bb.asDoubleBuffer().get());
        }

        out.println();
        out.println("<-- MyJapiTest.test2()");
    }
    
    static public void test3() {
        out.println("--> MyJapiTest.test3()");

        out.println();
        out.println("testing basic MyJapi functions: f3xxv(f2xxv()) ...");
        out.println();

        // XXX check NULL argument, array length != 1 arg

        if (false) {
            out.println("\ncalling f317v(f217v()); f317v(f217v())");
            final int nf217v0 = MyJapi.f217v();
            MyJapi.f317v(nf217v0);
            final int nf217v1 = MyJapi.f217v();
            MyJapi.f317v(nf217v1);
            assert (nf217v0 + 1 == nf217v1);
        }
        if (false) {
            out.println("\ncalling f337bb(f237bb()); f237v()");
            final int nf237v0 = MyJapi.f237v();
            final int[] nf337v = { nf237v0 };
            MyJapi.f337v(nf337v);
            assert (nf237v0 + 1 == nf337v[0]);
            final int nf237v1 = MyJapi.f237v();
            assert (nf237v1 == nf337v[0]);
        }

        for (int i = 0; i < 2; i++) {
            out.println("\ncalling f311v(f211v()); f311v(f211v())");
            final boolean nf211v0 = MyJapi.f211v();
            MyJapi.f311v(nf211v0);
            final boolean nf211v1 = MyJapi.f211v();
            MyJapi.f311v(nf211v1);
            assert (!nf211v0 == nf211v1);

            out.println("\ncalling f312v(f212v()); f312v(f212v())");
            final byte nf212v0 = MyJapi.f212v();
            MyJapi.f312v(nf212v0);
            final byte nf212v1 = MyJapi.f212v();
            MyJapi.f312v(nf212v1);
            assert (nf212v0 + 1 == nf212v1);

            out.println("\ncalling f313v(f213v()); f313v(f213v())");
            final byte nf213v0 = MyJapi.f213v();
            MyJapi.f313v(nf213v0);
            final byte nf213v1 = MyJapi.f213v();
            MyJapi.f313v(nf213v1);
            assert (nf213v0 + 1 == nf213v1);

            out.println("\ncalling f314v(f214v()); f314v(f214v())");
            final byte nf214v0 = MyJapi.f214v();
            MyJapi.f314v(nf214v0);
            final byte nf214v1 = MyJapi.f214v();
            MyJapi.f314v(nf214v1);
            assert (nf214v0 + 1 == nf214v1);

            out.println("\ncalling f315v(f215v()); f315v(f215v())");
            final short nf215v0 = MyJapi.f215v();
            MyJapi.f315v(nf215v0);
            final short nf215v1 = MyJapi.f215v();
            MyJapi.f315v(nf215v1);
            assert (nf215v0 + 1 == nf215v1);

            out.println("\ncalling f316v(f216v()); f316v(f216v())");
            final short nf216v0 = MyJapi.f216v();
            MyJapi.f316v(nf216v0);
            final short nf216v1 = MyJapi.f216v();
            MyJapi.f316v(nf216v1);
            assert (nf216v0 + 1 == nf216v1);

            out.println("\ncalling f317v(f217v()); f317v(f217v())");
            final int nf217v0 = MyJapi.f217v();
            MyJapi.f317v(nf217v0);
            final int nf217v1 = MyJapi.f217v();
            MyJapi.f317v(nf217v1);
            assert (nf217v0 + 1 == nf217v1);

            out.println("\ncalling f318v(f218v()); f318v(f218v())");
            final int nf218v0 = MyJapi.f218v();
            MyJapi.f318v(nf218v0);
            final int nf218v1 = MyJapi.f218v();
            MyJapi.f318v(nf218v1);
            assert (nf218v0 + 1 == nf218v1);

            out.println("\ncalling f321v(f221v()); f321v(f221v())");
            final long nf221v0 = MyJapi.f221v();
            MyJapi.f321v(nf221v0);
            final long nf221v1 = MyJapi.f221v();
            MyJapi.f321v(nf221v1);
            assert (nf221v0 + 1 == nf221v1);

            out.println("\ncalling f322v(f222v()); f322v(f222v())");
            final long nf222v0 = MyJapi.f222v();
            MyJapi.f322v(nf222v0);
            final long nf222v1 = MyJapi.f222v();
            MyJapi.f322v(nf222v1);
            assert (nf222v0 + 1 == nf222v1);

            out.println("\ncalling f323v(f223v()); f323v(f223v())");
            final float nf223v0 = MyJapi.f223v();
            MyJapi.f323v(nf223v0);
            final float nf223v1 = MyJapi.f223v();
            MyJapi.f323v(nf223v1);
            assert (nf223v0 + 1 == nf223v1);

            out.println("\ncalling f324v(f224v()); f324v(f224v())");
            final double nf224v0 = MyJapi.f224v();
            MyJapi.f324v(nf224v0);
            final double nf224v1 = MyJapi.f224v();
            MyJapi.f324v(nf224v1);
            assert (nf224v0 + 1 == nf224v1);
        }
        
        for (int i = 0; i < 2; i++) {
            out.println("\ncalling f331v(f231v()); f231v()");
            final boolean nf231v0 = MyJapi.f231v();
            final boolean[] nf331v = { nf231v0 };
            MyJapi.f331v(nf331v);
            assert (!nf231v0 == nf331v[0]);
            final boolean nf231v1 = MyJapi.f231v();
            assert (nf231v1 == nf331v[0]);

            out.println("\ncalling f332v(f232v()); f232v()");
            final byte nf232v0 = MyJapi.f232v();
            final byte[] nf332v = { nf232v0 };
            MyJapi.f332v(nf332v);
            assert (nf232v0 + 1 == nf332v[0]);
            final byte nf232v1 = MyJapi.f232v();
            assert (nf232v1 == nf332v[0]);

            out.println("\ncalling f333v(f233v()); f233v()");
            final byte nf233v0 = MyJapi.f233v();
            final byte[] nf333v = { nf233v0 };
            MyJapi.f333v(nf333v);
            assert (nf233v0 + 1 == nf333v[0]);
            final byte nf233v1 = MyJapi.f233v();
            assert (nf233v1 == nf333v[0]);

            out.println("\ncalling f334v(f234v()); f234v()");
            final byte nf234v0 = MyJapi.f234v();
            final byte[] nf334v = { nf234v0 };
            MyJapi.f334v(nf334v);
            assert (nf234v0 + 1 == nf334v[0]);
            final byte nf234v1 = MyJapi.f234v();
            assert (nf234v1 == nf334v[0]);

            out.println("\ncalling f335v(f235v()); f235v()");
            final short nf235v0 = MyJapi.f235v();
            final short[] nf335v = { nf235v0 };
            MyJapi.f335v(nf335v);
            assert (nf235v0 + 1 == nf335v[0]);
            final short nf235v1 = MyJapi.f235v();
            assert (nf235v1 == nf335v[0]);

            out.println("\ncalling f336v(f236v()); f236v()");
            final short nf236v0 = MyJapi.f236v();
            final short[] nf336v = { nf236v0 };
            MyJapi.f336v(nf336v);
            assert (nf236v0 + 1 == nf336v[0]);
            final short nf236v1 = MyJapi.f236v();
            assert (nf236v1 == nf336v[0]);

            out.println("\ncalling f337v(f237v()); f237v()");
            final int nf237v0 = MyJapi.f237v();
            final int[] nf337v = { nf237v0 };
            MyJapi.f337v(nf337v);
            assert (nf237v0 + 1 == nf337v[0]);
            final int nf237v1 = MyJapi.f237v();
            assert (nf237v1 == nf337v[0]);

            out.println("\ncalling f338v(f238v()); f238v()");
            final int nf238v0 = MyJapi.f238v();
            final int[] nf338v = { nf238v0 };
            MyJapi.f338v(nf338v);
            assert (nf238v0 + 1 == nf338v[0]);
            final int nf238v1 = MyJapi.f238v();
            assert (nf238v1 == nf338v[0]);

            out.println("\ncalling f341v(f241v()); f241v()");
            final long nf241v0 = MyJapi.f241v();
            final long[] nf341v = { nf241v0 };
            MyJapi.f341v(nf341v);
            assert (nf241v0 + 1 == nf341v[0]);
            final long nf241v1 = MyJapi.f241v();
            assert (nf241v1 == nf341v[0]);

            out.println("\ncalling f342v(f242v()); f242v()");
            final long nf242v0 = MyJapi.f242v();
            final long[] nf342v = { nf242v0 };
            MyJapi.f342v(nf342v);
            assert (nf242v0 + 1 == nf342v[0]);
            final long nf242v1 = MyJapi.f242v();
            assert (nf242v1 == nf342v[0]);

            out.println("\ncalling f343v(f243v()); f243v()");
            final float nf243v0 = MyJapi.f243v();
            final float[] nf343v = { nf243v0 };
            MyJapi.f343v(nf343v);
            assert (nf243v0 + 1 == nf343v[0]);
            final float nf243v1 = MyJapi.f243v();
            assert (nf243v1 == nf343v[0]);

            out.println("\ncalling f344v(f244v()); f244v()");
            final double nf244v0 = MyJapi.f244v();
            final double[] nf344v = { nf244v0 };
            MyJapi.f344v(nf344v);
            assert (nf244v0 + 1 == nf344v[0]);
            final double nf244v1 = MyJapi.f244v();
            assert (nf244v1 == nf344v[0]);
        }
        
        out.println();
        out.println("<-- MyJapiTest.test3()");
    }

    static public void test4() {
        out.println("--> MyJapiTest.test4()");

        out.println();
        out.println("testing instance wrappers: ...");
        int n = -1;
    
        out.println("\ncalling A.create()...");
        A a = A.create();
        out.println("... a = " + a);
        assert (a != null);

        out.println("\ncalling a.f0s()...");
        n = a.f0s();
        out.println("... a.f0s() = " + n);
        assert (n == 10);

        out.println("\ncalling A.f0n(a)...");
        n = A.f0n(a);
        out.println("... A.f0n(a) = " + n);
        assert (n == 11);

        out.println("\ncalling a.f0v()...");
        n = a.f0v();
        out.println("... a.f0v() = " + n);
        assert (n == 12);

        out.println("\ncalling B0...");
        n = B0.f0s();    
        out.println("... B0.f0s() = " + n);
        assert (n == 20);

        out.println("\ncalling a.getB0()...");
        B0 b0b0 = a.getB0();
        out.println("... b0b0 = " + b0b0);
        assert (b0b0 != null);

        out.println("\ncalling b0b0.f0s()...");
        n = b0b0.f0s();    
        out.println("... b0b0.f0s() = " + n);
        assert (n == 20);

        out.println("\ncalling B0.f0n(b0b0)...");
        n = B0.f0n(b0b0);
        out.println("... B0.f0n(b0b0) = " + n);
        assert (n == 21);

        out.println("\ncalling b0b0.f0v()...");
        n = b0b0.f0v();
        out.println("... b0b0.f0v() = " + n);
        assert (n == 22);

        out.println("\ncalling B1.f0s()...");
        n = B1.f0s();    
        out.println("... B1.f0s() = " + n);
        assert (n == 30);

        out.println("\ncalling a.getB1()...");
        B0 b0b1 = a.getB1();
        out.println("... b0b1 = " + b0b1);
        assert (b0b1 != null);

        out.println("\ncalling b0b1.f0s()...");
        n = b0b1.f0s();    
        out.println("... b0b1.f0s() = " + n);
        assert (n == 20);

        out.println("\ncalling B0.f0n(b0b1)...");
        n = B0.f0n(b0b1);
        out.println("... B0.f0n(b0b1) = " + n);
        assert (n == 21);

        out.println("\ncalling b0b1.f0v()...");
        n = b0b1.f0v();
        out.println("... b0b1.f0v() = " + n);
        assert (n == 32);

        out.println("\ncalling A.delete(A)...");
        A.delete(a);
        out.println("... a = " + a);
        assert (a != null);

        out.println("\ncalling A.return_ptr()...");
        A pa = A.return_ptr();
        assert (pa != null);
        
        out.println("\ncalling A.take_ptr()...");
        A.take_ptr(pa);
        
        out.println("\ncalling A.return_null_ptr()...");
        A p0 = A.return_null_ptr();
        assert (p0 == null);
        
        out.println("\ncalling A.take_null_ptr()...");
        A.take_null_ptr(p0);
        
        out.println("\ncalling A.return_ref()...");
        A ra = A.return_ref();
        assert (ra != null);
        
        out.println("\ncalling A.take_ref()...");
        A.take_ref(ra);
        
        out.println("\ncalling A.return_null_ref()...");
        try {
            A.return_null_ref();
            assert (false);
        } catch (AssertionError e) {
            out.println("... successfully caught: " + e);
        }
        
        out.println("\ncalling A.take_null_ref()...");
        try {
            A.take_null_ref(null);
            assert (false);
        } catch (IllegalArgumentException e) {
            out.println("... successfully caught: " + e);
        }
        
        out.println("\ncalling A.print()...");
        A.print(a);

        out.println();
        out.println("<-- MyJapiTest.test4()");
    };

    static public void test5() {
        out.println("--> MyJapiTest.test5()");

        out.println();
        out.println("testing n-ary array functions: g(), h() ...");
        int n = -1;

        out.println("\ncalling A.create()...");
        A a = A.create();
        out.println("... a = " + a);
        assert (a != null);

        A.h0();

        A.h1((byte)1);

        A.h2((byte)1, (short)2);

        A.h3((byte)1, (short)2, 3);

        n = A.h0r();
        assert (n == 0);
    
        n = A.h1r((byte)1);
        assert (n == 1);
    
        n = A.h2r((byte)1, (short)2);
        assert (n == 3);
    
        n = A.h3r((byte)1, (short)2, 3);
        assert (n == 6);
    
        A.g0c(a);

        A.g1c(a, (byte)1);

        A.g2c(a, (byte)1, (short)2);

        A.g3c(a, (byte)1, (short)2, 3);

        A.g0(a);

        A.g1(a, (byte)1);

        A.g2(a, (byte)1, (short)2);

        A.g3(a, (byte)1, (short)2, 3);

        n = A.g0rc(a);
        assert (n == 0);
    
        n = A.g1rc(a, (byte)1);
        assert (n == 1);
    
        n = A.g2rc(a, (byte)1, (short)2);
        assert (n == 3);
    
        n = A.g3rc(a, (byte)1, (short)2, 3);
        assert (n == 6);
    
        n = A.g0r(a);
        assert (n == 0);
    
        n = A.g1r(a, (byte)1);
        assert (n == 1);
    
        n = A.g2r(a, (byte)1, (short)2);
        assert (n == 3);
    
        n = A.g3r(a, (byte)1, (short)2, 3);
        assert (n == 6);

        out.println();
        out.println("<-- MyJapiTest.test5()");
    };
    
    static public void test6() {
        out.println("--> MyJapiTest.test6()");

        out.println();
        out.println("testing MyJapi String functions: s1xx(s0xx()) ...");
        out.println();

        out.println("\ncalling s112(s012())");
        MyJapi.s112(MyJapi.s012());

        out.println();
        out.println("<-- MyJapiTest.test6()");
    };

    static public void main(String[] args) 
    {
        out.println("--> MyJapiTest.main()");

        out.println();
        loadSystemLibrary("myjapi");

        if (true) {
            out.println();
            test0();
            out.println();
            test1();
            out.println();
            test2();
            out.println();
            test3();
            out.println();
            test4();
            out.println();
            test5();
            out.println();
            test6();
        } else {
            out.println();
            test4();
        }

        out.println();
        out.println("<-- MyJapiTest.main()");
    }
}
