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
 * MyJapi.java
 */

package myjapi;

//import java.math.BigInteger;
//import java.math.BigDecimal;
import java.nio.ByteBuffer;

public class MyJapi {

    // ----------------------------------------------------------------------
    // Mapping of void result/parameters
    // ----------------------------------------------------------------------

    static public native void f0();

    // ----------------------------------------------------------------------
    // String mappings of [const] char* result/parameters
    // ----------------------------------------------------------------------

    static public native String s012s();
    static public native void s112s(String p0);
    static public native void s152s(String p0);

    // mapping as result is supported, for s112(s032()) is in C++
    static public native String s032s();
    // mapping as parameters is not supported, for s132(s012()) is not in C++
    //static public native void s132s(String p0);
    //static public native void s172s(String p0);

    // ----------------------------------------------------------------------
    // ByteBuffer<size=0> mappings of void/char pointers
    // ----------------------------------------------------------------------

    static public native ByteBuffer s010bb0();
    static public native ByteBuffer s012bb0();
    static public native ByteBuffer s030bb0();
    static public native ByteBuffer s032bb0();
    
    static public native void s110bb0(ByteBuffer p0);
    static public native void s112bb0(ByteBuffer p0);
    static public native void s130bb0(ByteBuffer p0);
    static public native void s132bb0(ByteBuffer p0);
    static public native void s150bb0(ByteBuffer p0);
    static public native void s152bb0(ByteBuffer p0);
    static public native void s170bb0(ByteBuffer p0);
    static public native void s172bb0(ByteBuffer p0);

    // ----------------------------------------------------------------------
    // ByteBuffer<size=1> mappings of void/char pointers
    // ----------------------------------------------------------------------

    static public native ByteBuffer s010bb1();
    static public native ByteBuffer s012bb1();
    static public native ByteBuffer s030bb1();
    static public native ByteBuffer s032bb1();
    
    static public native void s110bb1(ByteBuffer p0);
    static public native void s112bb1(ByteBuffer p0);
    static public native void s130bb1(ByteBuffer p0);
    static public native void s132bb1(ByteBuffer p0);
    static public native void s150bb1(ByteBuffer p0);
    static public native void s152bb1(ByteBuffer p0);
    static public native void s170bb1(ByteBuffer p0);
    static public native void s172bb1(ByteBuffer p0);

    // ----------------------------------------------------------------------
    // ByteBuffer<size=0> mappings of NULL-allowed void/char pointers
    // ----------------------------------------------------------------------

    static public native ByteBuffer s210bb();
    static public native ByteBuffer s212bb();
    static public native ByteBuffer s230bb();
    static public native ByteBuffer s232bb();
    
    static public native void s310bb(ByteBuffer p0);
    static public native void s312bb(ByteBuffer p0);
    static public native void s330bb(ByteBuffer p0);
    static public native void s332bb(ByteBuffer p0);
    static public native void s350bb(ByteBuffer p0);
    static public native void s352bb(ByteBuffer p0);
    static public native void s370bb(ByteBuffer p0);
    static public native void s372bb(ByteBuffer p0);

    // ----------------------------------------------------------------------
    // Default mappings of primitive result/parameter types
    // ----------------------------------------------------------------------

    static public native int f019();
    static public native int f020();
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
    // ByteBuffer mappings of references of primitive result/parameter types
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
    // Value-Copy mappings of references of primitive result/parameter types
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
    // ByteBuffer<size=0> mappings of pointers to primitive types
    // ----------------------------------------------------------------------

    static public native ByteBuffer f411bb0();
    static public native ByteBuffer f412bb0();
    static public native ByteBuffer f413bb0();
    static public native ByteBuffer f414bb0();
    static public native ByteBuffer f415bb0();
    static public native ByteBuffer f416bb0();
    static public native ByteBuffer f417bb0();
    static public native ByteBuffer f418bb0();
    static public native ByteBuffer f421bb0();
    static public native ByteBuffer f422bb0();
    static public native ByteBuffer f423bb0();
    static public native ByteBuffer f424bb0();

    static public native ByteBuffer f431bb0();
    static public native ByteBuffer f432bb0();
    static public native ByteBuffer f433bb0();
    static public native ByteBuffer f434bb0();
    static public native ByteBuffer f435bb0();
    static public native ByteBuffer f436bb0();
    static public native ByteBuffer f437bb0();
    static public native ByteBuffer f438bb0();
    static public native ByteBuffer f441bb0();
    static public native ByteBuffer f442bb0();
    static public native ByteBuffer f443bb0();
    static public native ByteBuffer f444bb0();



    static public native void f511bb0(ByteBuffer p0);
    static public native void f512bb0(ByteBuffer p0);
    static public native void f513bb0(ByteBuffer p0);
    static public native void f514bb0(ByteBuffer p0);
    static public native void f515bb0(ByteBuffer p0);
    static public native void f516bb0(ByteBuffer p0);
    static public native void f517bb0(ByteBuffer p0);
    static public native void f518bb0(ByteBuffer p0);
    static public native void f521bb0(ByteBuffer p0);
    static public native void f522bb0(ByteBuffer p0);
    static public native void f523bb0(ByteBuffer p0);
    static public native void f524bb0(ByteBuffer p0);

    static public native void f531bb0(ByteBuffer p0);
    static public native void f532bb0(ByteBuffer p0);
    static public native void f533bb0(ByteBuffer p0);
    static public native void f534bb0(ByteBuffer p0);
    static public native void f535bb0(ByteBuffer p0);
    static public native void f536bb0(ByteBuffer p0);
    static public native void f537bb0(ByteBuffer p0);
    static public native void f538bb0(ByteBuffer p0);
    static public native void f541bb0(ByteBuffer p0);
    static public native void f542bb0(ByteBuffer p0);
    static public native void f543bb0(ByteBuffer p0);
    static public native void f544bb0(ByteBuffer p0);

    static public native void f551bb0(ByteBuffer p0);
    static public native void f552bb0(ByteBuffer p0);
    static public native void f553bb0(ByteBuffer p0);
    static public native void f554bb0(ByteBuffer p0);
    static public native void f555bb0(ByteBuffer p0);
    static public native void f556bb0(ByteBuffer p0);
    static public native void f557bb0(ByteBuffer p0);
    static public native void f558bb0(ByteBuffer p0);
    static public native void f561bb0(ByteBuffer p0);
    static public native void f562bb0(ByteBuffer p0);
    static public native void f563bb0(ByteBuffer p0);
    static public native void f564bb0(ByteBuffer p0);

    static public native void f571bb0(ByteBuffer p0);
    static public native void f572bb0(ByteBuffer p0);
    static public native void f573bb0(ByteBuffer p0);
    static public native void f574bb0(ByteBuffer p0);
    static public native void f575bb0(ByteBuffer p0);
    static public native void f576bb0(ByteBuffer p0);
    static public native void f577bb0(ByteBuffer p0);
    static public native void f578bb0(ByteBuffer p0);
    static public native void f581bb0(ByteBuffer p0);
    static public native void f582bb0(ByteBuffer p0);
    static public native void f583bb0(ByteBuffer p0);
    static public native void f584bb0(ByteBuffer p0);

    // ----------------------------------------------------------------------
    // ByteBuffer<sizeof(C)> mappings of pointers to primitive types
    // ----------------------------------------------------------------------

    static public native ByteBuffer f411bb1();
    static public native ByteBuffer f412bb1();
    static public native ByteBuffer f413bb1();
    static public native ByteBuffer f414bb1();
    static public native ByteBuffer f415bb1();
    static public native ByteBuffer f416bb1();
    static public native ByteBuffer f417bb1();
    static public native ByteBuffer f418bb1();
    static public native ByteBuffer f421bb1();
    static public native ByteBuffer f422bb1();
    static public native ByteBuffer f423bb1();
    static public native ByteBuffer f424bb1();

    static public native ByteBuffer f431bb1();
    static public native ByteBuffer f432bb1();
    static public native ByteBuffer f433bb1();
    static public native ByteBuffer f434bb1();
    static public native ByteBuffer f435bb1();
    static public native ByteBuffer f436bb1();
    static public native ByteBuffer f437bb1();
    static public native ByteBuffer f438bb1();
    static public native ByteBuffer f441bb1();
    static public native ByteBuffer f442bb1();
    static public native ByteBuffer f443bb1();
    static public native ByteBuffer f444bb1();



    static public native void f511bb1(ByteBuffer p0);
    static public native void f512bb1(ByteBuffer p0);
    static public native void f513bb1(ByteBuffer p0);
    static public native void f514bb1(ByteBuffer p0);
    static public native void f515bb1(ByteBuffer p0);
    static public native void f516bb1(ByteBuffer p0);
    static public native void f517bb1(ByteBuffer p0);
    static public native void f518bb1(ByteBuffer p0);
    static public native void f521bb1(ByteBuffer p0);
    static public native void f522bb1(ByteBuffer p0);
    static public native void f523bb1(ByteBuffer p0);
    static public native void f524bb1(ByteBuffer p0);

    static public native void f531bb1(ByteBuffer p0);
    static public native void f532bb1(ByteBuffer p0);
    static public native void f533bb1(ByteBuffer p0);
    static public native void f534bb1(ByteBuffer p0);
    static public native void f535bb1(ByteBuffer p0);
    static public native void f536bb1(ByteBuffer p0);
    static public native void f537bb1(ByteBuffer p0);
    static public native void f538bb1(ByteBuffer p0);
    static public native void f541bb1(ByteBuffer p0);
    static public native void f542bb1(ByteBuffer p0);
    static public native void f543bb1(ByteBuffer p0);
    static public native void f544bb1(ByteBuffer p0);

    static public native void f551bb1(ByteBuffer p0);
    static public native void f552bb1(ByteBuffer p0);
    static public native void f553bb1(ByteBuffer p0);
    static public native void f554bb1(ByteBuffer p0);
    static public native void f555bb1(ByteBuffer p0);
    static public native void f556bb1(ByteBuffer p0);
    static public native void f557bb1(ByteBuffer p0);
    static public native void f558bb1(ByteBuffer p0);
    static public native void f561bb1(ByteBuffer p0);
    static public native void f562bb1(ByteBuffer p0);
    static public native void f563bb1(ByteBuffer p0);
    static public native void f564bb1(ByteBuffer p0);

    static public native void f571bb1(ByteBuffer p0);
    static public native void f572bb1(ByteBuffer p0);
    static public native void f573bb1(ByteBuffer p0);
    static public native void f574bb1(ByteBuffer p0);
    static public native void f575bb1(ByteBuffer p0);
    static public native void f576bb1(ByteBuffer p0);
    static public native void f577bb1(ByteBuffer p0);
    static public native void f578bb1(ByteBuffer p0);
    static public native void f581bb1(ByteBuffer p0);
    static public native void f582bb1(ByteBuffer p0);
    static public native void f583bb1(ByteBuffer p0);
    static public native void f584bb1(ByteBuffer p0);

    // ----------------------------------------------------------------------
    // Array<size=1> mappings of pointers to primitive types
    // ----------------------------------------------------------------------

    static public native boolean[] f411v1();
    static public native byte[] f412v1();
    static public native byte[] f413v1();
    static public native byte[] f414v1();
    static public native short[] f415v1();
    static public native short[] f416v1();
    static public native int[] f417v1();
    static public native int[] f418v1();
    static public native long[] f421v1();
    static public native long[] f422v1();
    static public native float[] f423v1();
    static public native double[] f424v1();

    static public native boolean[] f431v1();
    static public native byte[] f432v1();
    static public native byte[] f433v1();
    static public native byte[] f434v1();
    static public native short[] f435v1();
    static public native short[] f436v1();
    static public native int[] f437v1();
    static public native int[] f438v1();
    static public native long[] f441v1();
    static public native long[] f442v1();
    static public native float[] f443v1();
    static public native double[] f444v1();



    static public native void f511v1(boolean[] p0);
    static public native void f512v1(byte[] p0);
    static public native void f513v1(byte[] p0);
    static public native void f514v1(byte[] p0);
    static public native void f515v1(short[] p0);
    static public native void f516v1(short[] p0);
    static public native void f517v1(int[] p0);
    static public native void f518v1(int[] p0);
    static public native void f521v1(long[] p0);
    static public native void f522v1(long[] p0);
    static public native void f523v1(float[] p0);
    static public native void f524v1(double[] p0);

    static public native void f531v1(boolean[] p0);
    static public native void f532v1(byte[] p0);
    static public native void f533v1(byte[] p0);
    static public native void f534v1(byte[] p0);
    static public native void f535v1(short[] p0);
    static public native void f536v1(short[] p0);
    static public native void f537v1(int[] p0);
    static public native void f538v1(int[] p0);
    static public native void f541v1(long[] p0);
    static public native void f542v1(long[] p0);
    static public native void f543v1(float[] p0);
    static public native void f544v1(double[] p0);

    static public native void f551v1(boolean[] p0);
    static public native void f552v1(byte[] p0);
    static public native void f553v1(byte[] p0);
    static public native void f554v1(byte[] p0);
    static public native void f555v1(short[] p0);
    static public native void f556v1(short[] p0);
    static public native void f557v1(int[] p0);
    static public native void f558v1(int[] p0);
    static public native void f561v1(long[] p0);
    static public native void f562v1(long[] p0);
    static public native void f563v1(float[] p0);
    static public native void f564v1(double[] p0);

    static public native void f571v1(boolean[] p0);
    static public native void f572v1(byte[] p0);
    static public native void f573v1(byte[] p0);
    static public native void f574v1(byte[] p0);
    static public native void f575v1(short[] p0);
    static public native void f576v1(short[] p0);
    static public native void f577v1(int[] p0);
    static public native void f578v1(int[] p0);
    static public native void f581v1(long[] p0);
    static public native void f582v1(long[] p0);
    static public native void f583v1(float[] p0);
    static public native void f584v1(double[] p0);

    // ----------------------------------------------------------------------
    // Nullable ByteBuffer<size=0> mappings of pointers to primitive types
    // ----------------------------------------------------------------------

    static public native ByteBuffer f611bb0();
    static public native ByteBuffer f612bb0();
    static public native ByteBuffer f613bb0();
    static public native ByteBuffer f614bb0();
    static public native ByteBuffer f615bb0();
    static public native ByteBuffer f616bb0();
    static public native ByteBuffer f617bb0();
    static public native ByteBuffer f618bb0();
    static public native ByteBuffer f621bb0();
    static public native ByteBuffer f622bb0();
    static public native ByteBuffer f623bb0();
    static public native ByteBuffer f624bb0();

    static public native ByteBuffer f631bb0();
    static public native ByteBuffer f632bb0();
    static public native ByteBuffer f633bb0();
    static public native ByteBuffer f634bb0();
    static public native ByteBuffer f635bb0();
    static public native ByteBuffer f636bb0();
    static public native ByteBuffer f637bb0();
    static public native ByteBuffer f638bb0();
    static public native ByteBuffer f641bb0();
    static public native ByteBuffer f642bb0();
    static public native ByteBuffer f643bb0();
    static public native ByteBuffer f644bb0();



    static public native void f711bb0(ByteBuffer p0);
    static public native void f712bb0(ByteBuffer p0);
    static public native void f713bb0(ByteBuffer p0);
    static public native void f714bb0(ByteBuffer p0);
    static public native void f715bb0(ByteBuffer p0);
    static public native void f716bb0(ByteBuffer p0);
    static public native void f717bb0(ByteBuffer p0);
    static public native void f718bb0(ByteBuffer p0);
    static public native void f721bb0(ByteBuffer p0);
    static public native void f722bb0(ByteBuffer p0);
    static public native void f723bb0(ByteBuffer p0);
    static public native void f724bb0(ByteBuffer p0);

    static public native void f731bb0(ByteBuffer p0);
    static public native void f732bb0(ByteBuffer p0);
    static public native void f733bb0(ByteBuffer p0);
    static public native void f734bb0(ByteBuffer p0);
    static public native void f735bb0(ByteBuffer p0);
    static public native void f736bb0(ByteBuffer p0);
    static public native void f737bb0(ByteBuffer p0);
    static public native void f738bb0(ByteBuffer p0);
    static public native void f741bb0(ByteBuffer p0);
    static public native void f742bb0(ByteBuffer p0);
    static public native void f743bb0(ByteBuffer p0);
    static public native void f744bb0(ByteBuffer p0);

    static public native void f751bb0(ByteBuffer p0);
    static public native void f752bb0(ByteBuffer p0);
    static public native void f753bb0(ByteBuffer p0);
    static public native void f754bb0(ByteBuffer p0);
    static public native void f755bb0(ByteBuffer p0);
    static public native void f756bb0(ByteBuffer p0);
    static public native void f757bb0(ByteBuffer p0);
    static public native void f758bb0(ByteBuffer p0);
    static public native void f761bb0(ByteBuffer p0);
    static public native void f762bb0(ByteBuffer p0);
    static public native void f763bb0(ByteBuffer p0);
    static public native void f764bb0(ByteBuffer p0);

    static public native void f771bb0(ByteBuffer p0);
    static public native void f772bb0(ByteBuffer p0);
    static public native void f773bb0(ByteBuffer p0);
    static public native void f774bb0(ByteBuffer p0);
    static public native void f775bb0(ByteBuffer p0);
    static public native void f776bb0(ByteBuffer p0);
    static public native void f777bb0(ByteBuffer p0);
    static public native void f778bb0(ByteBuffer p0);
    static public native void f781bb0(ByteBuffer p0);
    static public native void f782bb0(ByteBuffer p0);
    static public native void f783bb0(ByteBuffer p0);
    static public native void f784bb0(ByteBuffer p0);

    // ----------------------------------------------------------------------
    // Nullable ByteBuffer<sizeof(C)> mappings of pointers to primitive types
    // ----------------------------------------------------------------------

    static public native ByteBuffer f611bb1();
    static public native ByteBuffer f612bb1();
    static public native ByteBuffer f613bb1();
    static public native ByteBuffer f614bb1();
    static public native ByteBuffer f615bb1();
    static public native ByteBuffer f616bb1();
    static public native ByteBuffer f617bb1();
    static public native ByteBuffer f618bb1();
    static public native ByteBuffer f621bb1();
    static public native ByteBuffer f622bb1();
    static public native ByteBuffer f623bb1();
    static public native ByteBuffer f624bb1();

    static public native ByteBuffer f631bb1();
    static public native ByteBuffer f632bb1();
    static public native ByteBuffer f633bb1();
    static public native ByteBuffer f634bb1();
    static public native ByteBuffer f635bb1();
    static public native ByteBuffer f636bb1();
    static public native ByteBuffer f637bb1();
    static public native ByteBuffer f638bb1();
    static public native ByteBuffer f641bb1();
    static public native ByteBuffer f642bb1();
    static public native ByteBuffer f643bb1();
    static public native ByteBuffer f644bb1();



    static public native void f711bb1(ByteBuffer p0);
    static public native void f712bb1(ByteBuffer p0);
    static public native void f713bb1(ByteBuffer p0);
    static public native void f714bb1(ByteBuffer p0);
    static public native void f715bb1(ByteBuffer p0);
    static public native void f716bb1(ByteBuffer p0);
    static public native void f717bb1(ByteBuffer p0);
    static public native void f718bb1(ByteBuffer p0);
    static public native void f721bb1(ByteBuffer p0);
    static public native void f722bb1(ByteBuffer p0);
    static public native void f723bb1(ByteBuffer p0);
    static public native void f724bb1(ByteBuffer p0);

    static public native void f731bb1(ByteBuffer p0);
    static public native void f732bb1(ByteBuffer p0);
    static public native void f733bb1(ByteBuffer p0);
    static public native void f734bb1(ByteBuffer p0);
    static public native void f735bb1(ByteBuffer p0);
    static public native void f736bb1(ByteBuffer p0);
    static public native void f737bb1(ByteBuffer p0);
    static public native void f738bb1(ByteBuffer p0);
    static public native void f741bb1(ByteBuffer p0);
    static public native void f742bb1(ByteBuffer p0);
    static public native void f743bb1(ByteBuffer p0);
    static public native void f744bb1(ByteBuffer p0);

    static public native void f751bb1(ByteBuffer p0);
    static public native void f752bb1(ByteBuffer p0);
    static public native void f753bb1(ByteBuffer p0);
    static public native void f754bb1(ByteBuffer p0);
    static public native void f755bb1(ByteBuffer p0);
    static public native void f756bb1(ByteBuffer p0);
    static public native void f757bb1(ByteBuffer p0);
    static public native void f758bb1(ByteBuffer p0);
    static public native void f761bb1(ByteBuffer p0);
    static public native void f762bb1(ByteBuffer p0);
    static public native void f763bb1(ByteBuffer p0);
    static public native void f764bb1(ByteBuffer p0);

    static public native void f771bb1(ByteBuffer p0);
    static public native void f772bb1(ByteBuffer p0);
    static public native void f773bb1(ByteBuffer p0);
    static public native void f774bb1(ByteBuffer p0);
    static public native void f775bb1(ByteBuffer p0);
    static public native void f776bb1(ByteBuffer p0);
    static public native void f777bb1(ByteBuffer p0);
    static public native void f778bb1(ByteBuffer p0);
    static public native void f781bb1(ByteBuffer p0);
    static public native void f782bb1(ByteBuffer p0);
    static public native void f783bb1(ByteBuffer p0);
    static public native void f784bb1(ByteBuffer p0);

    // ----------------------------------------------------------------------
    // Nullable Array<size=0> mappings of pointers to primitive types
    // ----------------------------------------------------------------------

    static public native boolean[] f611v0();
    static public native byte[] f612v0();
    static public native byte[] f613v0();
    static public native byte[] f614v0();
    static public native short[] f615v0();
    static public native short[] f616v0();
    static public native int[] f617v0();
    static public native int[] f618v0();
    static public native long[] f621v0();
    static public native long[] f622v0();
    static public native float[] f623v0();
    static public native double[] f624v0();

    static public native boolean[] f631v0();
    static public native byte[] f632v0();
    static public native byte[] f633v0();
    static public native byte[] f634v0();
    static public native short[] f635v0();
    static public native short[] f636v0();
    static public native int[] f637v0();
    static public native int[] f638v0();
    static public native long[] f641v0();
    static public native long[] f642v0();
    static public native float[] f643v0();
    static public native double[] f644v0();



    static public native void f711v0(boolean[] p0);
    static public native void f712v0(byte[] p0);
    static public native void f713v0(byte[] p0);
    static public native void f714v0(byte[] p0);
    static public native void f715v0(short[] p0);
    static public native void f716v0(short[] p0);
    static public native void f717v0(int[] p0);
    static public native void f718v0(int[] p0);
    static public native void f721v0(long[] p0);
    static public native void f722v0(long[] p0);
    static public native void f723v0(float[] p0);
    static public native void f724v0(double[] p0);

    static public native void f731v0(boolean[] p0);
    static public native void f732v0(byte[] p0);
    static public native void f733v0(byte[] p0);
    static public native void f734v0(byte[] p0);
    static public native void f735v0(short[] p0);
    static public native void f736v0(short[] p0);
    static public native void f737v0(int[] p0);
    static public native void f738v0(int[] p0);
    static public native void f741v0(long[] p0);
    static public native void f742v0(long[] p0);
    static public native void f743v0(float[] p0);
    static public native void f744v0(double[] p0);

    static public native void f751v0(boolean[] p0);
    static public native void f752v0(byte[] p0);
    static public native void f753v0(byte[] p0);
    static public native void f754v0(byte[] p0);
    static public native void f755v0(short[] p0);
    static public native void f756v0(short[] p0);
    static public native void f757v0(int[] p0);
    static public native void f758v0(int[] p0);
    static public native void f761v0(long[] p0);
    static public native void f762v0(long[] p0);
    static public native void f763v0(float[] p0);
    static public native void f764v0(double[] p0);

    static public native void f771v0(boolean[] p0);
    static public native void f772v0(byte[] p0);
    static public native void f773v0(byte[] p0);
    static public native void f774v0(byte[] p0);
    static public native void f775v0(short[] p0);
    static public native void f776v0(short[] p0);
    static public native void f777v0(int[] p0);
    static public native void f778v0(int[] p0);
    static public native void f781v0(long[] p0);
    static public native void f782v0(long[] p0);
    static public native void f783v0(float[] p0);
    static public native void f784v0(double[] p0);

    // ----------------------------------------------------------------------
    // Nullable Array<size=1> mappings of pointers to primitive types
    // ----------------------------------------------------------------------

    static public native boolean[] f611v1();
    static public native byte[] f612v1();
    static public native byte[] f613v1();
    static public native byte[] f614v1();
    static public native short[] f615v1();
    static public native short[] f616v1();
    static public native int[] f617v1();
    static public native int[] f618v1();
    static public native long[] f621v1();
    static public native long[] f622v1();
    static public native float[] f623v1();
    static public native double[] f624v1();

    static public native boolean[] f631v1();
    static public native byte[] f632v1();
    static public native byte[] f633v1();
    static public native byte[] f634v1();
    static public native short[] f635v1();
    static public native short[] f636v1();
    static public native int[] f637v1();
    static public native int[] f638v1();
    static public native long[] f641v1();
    static public native long[] f642v1();
    static public native float[] f643v1();
    static public native double[] f644v1();



    static public native void f711v1(boolean[] p0);
    static public native void f712v1(byte[] p0);
    static public native void f713v1(byte[] p0);
    static public native void f714v1(byte[] p0);
    static public native void f715v1(short[] p0);
    static public native void f716v1(short[] p0);
    static public native void f717v1(int[] p0);
    static public native void f718v1(int[] p0);
    static public native void f721v1(long[] p0);
    static public native void f722v1(long[] p0);
    static public native void f723v1(float[] p0);
    static public native void f724v1(double[] p0);

    static public native void f731v1(boolean[] p0);
    static public native void f732v1(byte[] p0);
    static public native void f733v1(byte[] p0);
    static public native void f734v1(byte[] p0);
    static public native void f735v1(short[] p0);
    static public native void f736v1(short[] p0);
    static public native void f737v1(int[] p0);
    static public native void f738v1(int[] p0);
    static public native void f741v1(long[] p0);
    static public native void f742v1(long[] p0);
    static public native void f743v1(float[] p0);
    static public native void f744v1(double[] p0);

    static public native void f751v1(boolean[] p0);
    static public native void f752v1(byte[] p0);
    static public native void f753v1(byte[] p0);
    static public native void f754v1(byte[] p0);
    static public native void f755v1(short[] p0);
    static public native void f756v1(short[] p0);
    static public native void f757v1(int[] p0);
    static public native void f758v1(int[] p0);
    static public native void f761v1(long[] p0);
    static public native void f762v1(long[] p0);
    static public native void f763v1(float[] p0);
    static public native void f764v1(double[] p0);

    static public native void f771v1(boolean[] p0);
    static public native void f772v1(byte[] p0);
    static public native void f773v1(byte[] p0);
    static public native void f774v1(byte[] p0);
    static public native void f775v1(short[] p0);
    static public native void f776v1(short[] p0);
    static public native void f777v1(int[] p0);
    static public native void f778v1(int[] p0);
    static public native void f781v1(long[] p0);
    static public native void f782v1(long[] p0);
    static public native void f783v1(float[] p0);
    static public native void f784v1(double[] p0);

    // ----------------------------------------------------------------------

    // XXX add BigInteger, BigDecimal mappings

    //static public native void f140(BigInteger p0);
    //static public native void f141(BigInteger p0);
    // mapping to BigDecimal not supported at this time
    //static public native void f145(BigDecimal p0);

    // ----------------------------------------------------------------------
}
