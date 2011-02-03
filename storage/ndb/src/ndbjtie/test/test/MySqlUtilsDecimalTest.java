/*
 Copyright 2010 Sun Microsystems, Inc.
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
 * MySqlUtilsDecimalTest.java
 */

package test;

import java.io.PrintWriter;
import java.nio.ByteBuffer;
import java.nio.charset.Charset;

import com.mysql.ndbjtie.mysql.Utils;

/**
 * Tests the basic functioning of the NdbJTie libary: mysql decimal utils
 * for string-binary conversions.
 */
public class MySqlUtilsDecimalTest extends JTieTestBase {

    public void test_s2b2s(String s, int prec, int scale) {
        out.print("    [" + prec + "/" + scale + "] '" + s + "' => ");
        out.flush();

        // write string buffer
        final ByteBuffer sbuf = ByteBuffer.allocateDirect(128);
        final byte[] b;
        try {
            b = s.getBytes("US-ASCII");
        } catch (java.io.UnsupportedEncodingException ex) {
            throw new RuntimeException(ex);
        }
        assert (s.equals(new String(b)));
        assert (b.length < sbuf.capacity());
        sbuf.put(b);
        assert (b.length == sbuf.position());
        sbuf.flip();
        assert (b.length == sbuf.limit());

        // clear binary buffer
        final ByteBuffer bbuf = ByteBuffer.allocateDirect(128);
        bbuf.clear();
        for (int i = 0; i < bbuf.capacity(); i++) bbuf.put((byte)0);
        bbuf.rewind();

        // string->binary
        assert (sbuf.position() == 0);
        assert (bbuf.position() == 0);
        final int r1 = Utils.decimal_str2bin(sbuf, sbuf.capacity(),
                                             prec, scale,
                                             bbuf, bbuf.capacity());
        if (r1 != Utils.E_DEC_OK) {
            out.println("decimal_str2bin() returned: " + r1);
            return;
        }

        // clear string buffer
        sbuf.clear();
        for (int i = 0; i < sbuf.capacity(); i++) sbuf.put((byte)0);
        sbuf.rewind();
        
        // binary->string
        assert (bbuf.position() == 0);
        assert (sbuf.position() == 0);
        final int r2 = Utils.decimal_bin2str(bbuf, bbuf.capacity(),
                                             prec, scale,
                                             sbuf, sbuf.capacity());
        if (r2 != Utils.E_DEC_OK) {
            out.println("decimal_bin2str() returned: " + r2);
            return;
        }

        // read string buffer
        assert (sbuf.position() == 0);
        sbuf.limit(prec);
        final String t = Charset.forName("US-ASCII").decode(sbuf).toString();

        out.println("'" + t + "'");
    }

    public void test() {
        out.println("--> MySqlUtilsDecimalTest.test()");

        // load native library
        loadSystemLibrary("ndbclient");

        test_s2b2s("3.3", 2, 1);
        test_s2b2s("124.000", 20, 4);
        test_s2b2s("-11", 14, 1);
        test_s2b2s("1.123456000000000", 20, 16);
        test_s2b2s("0", 20, 10);
        test_s2b2s("1 ", 20, 10);
        test_s2b2s("1,35", 20, 10);
        test_s2b2s("text",20, 10);

        out.println();
        out.println("<-- MySqlUtilsDecimalTest.test()");
    };
    
    static public void main(String[] args) throws Exception {
        out.println("--> MySqlUtilsDecimalTest.main()");

        out.println();
        MySqlUtilsDecimalTest test = new MySqlUtilsDecimalTest();
        test.test();
        
        out.println();
        out.println("<-- MySqlUtilsDecimalTest.main()");
    }
}
