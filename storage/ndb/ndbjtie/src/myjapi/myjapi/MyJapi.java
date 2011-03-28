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
 * MyJapi.java
 */

package myjapi;

//import java.math.BigInteger;
//import java.math.BigDecimal;
import java.nio.ByteBuffer;

// ---------------------------------------------------------------------------
// generatable Java wrapper class
// ---------------------------------------------------------------------------

public class MyJapi {

    static public native void f0();

    // ----------------------------------------------------------------------
    // string mappings of value result/parameter types
    // ----------------------------------------------------------------------

    static public native String s012();
    static public native void s112(String p0);

    // ----------------------------------------------------------------------
    // default mappings of value result/parameter types
    // ----------------------------------------------------------------------

    static public native boolean f011();
    static public native byte f012();
    static public native byte f013();
    static public native byte f014();
    static public native short f015();
    static public native short f016();
    static public native int f017();
    static public native int f018();
    static public native int f019();
    static public native int f020();
    static public native long f021();
    static public native long f022();
    static public native float f023();
    static public native double f024();
    static public native double f025();

    static public native void f111(boolean p0);
    static public native void f112(byte p0);
    static public native void f113(byte p0);
    static public native void f114(byte p0);
    static public native void f115(short p0);
    static public native void f116(short p0);
    static public native void f117(int p0);
    static public native void f118(int p0);
    static public native void f119(int p0);
    static public native void f120(int p0);
    static public native void f121(long p0);
    static public native void f122(long p0);
    static public native void f123(float p0);
    static public native void f124(double p0);
    static public native void f125(double p0);

    static public native boolean f031();
    static public native byte f032();
    static public native byte f033();
    static public native byte f034();
    static public native short f035();
    static public native short f036();
    static public native int f037();
    static public native int f038();
    static public native int f039();
    static public native int f040();
    static public native long f041();
    static public native long f042();
    static public native float f043();
    static public native double f044();
    static public native double f045();

    static public native void f131(boolean p0);
    static public native void f132(byte p0);
    static public native void f133(byte p0);
    static public native void f134(byte p0);
    static public native void f135(short p0);
    static public native void f136(short p0);
    static public native void f137(int p0);
    static public native void f138(int p0);
    static public native void f139(int p0);
    static public native void f140(int p0);
    static public native void f141(long p0);
    static public native void f142(long p0);
    static public native void f143(float p0);
    static public native void f144(double p0);
    static public native void f145(double p0);

    // ----------------------------------------------------------------------
    // ByteBuffer mappings of object reference types
    // ----------------------------------------------------------------------

    static public native ByteBuffer f211bb();
    static public native ByteBuffer f212bb();
    static public native ByteBuffer f213bb();
    static public native ByteBuffer f214bb();
    static public native ByteBuffer f215bb();
    static public native ByteBuffer f216bb();
    static public native ByteBuffer f217bb();
    static public native ByteBuffer f218bb();
    static public native ByteBuffer f219bb();
    static public native ByteBuffer f220bb();
    static public native ByteBuffer f221bb();
    static public native ByteBuffer f222bb();
    static public native ByteBuffer f223bb();
    static public native ByteBuffer f224bb();
    static public native ByteBuffer f225bb();

    static public native void f311bb(ByteBuffer p0);
    static public native void f312bb(ByteBuffer p0);
    static public native void f313bb(ByteBuffer p0);
    static public native void f314bb(ByteBuffer p0);
    static public native void f315bb(ByteBuffer p0);
    static public native void f316bb(ByteBuffer p0);
    static public native void f317bb(ByteBuffer p0);
    static public native void f318bb(ByteBuffer p0);
    static public native void f319bb(ByteBuffer p0);
    static public native void f320bb(ByteBuffer p0);
    static public native void f321bb(ByteBuffer p0);
    static public native void f322bb(ByteBuffer p0);
    static public native void f323bb(ByteBuffer p0);
    static public native void f324bb(ByteBuffer p0);
    static public native void f325bb(ByteBuffer p0);

    static public native ByteBuffer f231bb();
    static public native ByteBuffer f232bb();
    static public native ByteBuffer f233bb();
    static public native ByteBuffer f234bb();
    static public native ByteBuffer f235bb();
    static public native ByteBuffer f236bb();
    static public native ByteBuffer f237bb();
    static public native ByteBuffer f238bb();
    static public native ByteBuffer f239bb();
    static public native ByteBuffer f240bb();
    static public native ByteBuffer f241bb();
    static public native ByteBuffer f242bb();
    static public native ByteBuffer f243bb();
    static public native ByteBuffer f244bb();
    static public native ByteBuffer f245bb();

    static public native void f331bb(ByteBuffer p0);
    static public native void f332bb(ByteBuffer p0);
    static public native void f333bb(ByteBuffer p0);
    static public native void f334bb(ByteBuffer p0);
    static public native void f335bb(ByteBuffer p0);
    static public native void f336bb(ByteBuffer p0);
    static public native void f337bb(ByteBuffer p0);
    static public native void f338bb(ByteBuffer p0);
    static public native void f339bb(ByteBuffer p0);
    static public native void f340bb(ByteBuffer p0);
    static public native void f341bb(ByteBuffer p0);
    static public native void f342bb(ByteBuffer p0);
    static public native void f343bb(ByteBuffer p0);
    static public native void f344bb(ByteBuffer p0);
    static public native void f345bb(ByteBuffer p0);

    // ----------------------------------------------------------------------
    // value-copy mappings of object reference types
    // ----------------------------------------------------------------------

    static public native boolean f211v();
    static public native byte f212v();
    static public native byte f213v();
    static public native byte f214v();
    static public native short f215v();
    static public native short f216v();
    static public native int f217v();
    static public native int f218v();
    static public native long f221v();
    static public native long f222v();
    static public native float f223v();
    static public native double f224v();

    static public native void f311v(boolean p0);
    static public native void f312v(byte p0);
    static public native void f313v(byte p0);
    static public native void f314v(byte p0);
    static public native void f315v(short p0);
    static public native void f316v(short p0);
    static public native void f317v(int p0);
    static public native void f318v(int p0);
    static public native void f321v(long p0);
    static public native void f322v(long p0);
    static public native void f323v(float p0);
    static public native void f324v(double p0);

    static public native boolean f231v();
    static public native byte f232v();
    static public native byte f233v();
    static public native byte f234v();
    static public native short f235v();
    static public native short f236v();
    static public native int f237v();
    static public native int f238v();
    static public native long f241v();
    static public native long f242v();
    static public native float f243v();
    static public native double f244v();

    static public native void f331v(boolean[] p0);
    static public native void f332v(byte[] p0);
    static public native void f333v(byte[] p0);
    static public native void f334v(byte[] p0);
    static public native void f335v(short[] p0);
    static public native void f336v(short[] p0);
    static public native void f337v(int[] p0);
    static public native void f338v(int[] p0);
    static public native void f341v(long[] p0);
    static public native void f342v(long[] p0);
    static public native void f343v(float[] p0);
    static public native void f344v(double[] p0);

    // ----------------------------------------------------------------------

    //static public native void f140(BigInteger p0);
    //static public native void f141(BigInteger p0);
    // mapping to BigDecimal not supported at this time
    //static public native void f145(BigDecimal p0);

    // ----------------------------------------------------------------------
}
