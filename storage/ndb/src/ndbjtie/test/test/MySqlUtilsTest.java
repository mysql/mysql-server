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
 * MySqlUtilsTest.java
 */

package test;

import java.nio.ByteBuffer;
import java.nio.CharBuffer;
import java.nio.charset.Charset;
import java.util.Arrays;

import com.mysql.ndbjtie.mysql.Utils;
import com.mysql.ndbjtie.mysql.CharsetMap;
import com.mysql.ndbjtie.mysql.CharsetMapConst;

/**
 * Tests NdbJTie's mysql utilities: CharsetMap utils for character set
 * conversions, decimal utils for string-binary conversions.
 */
public class MySqlUtilsTest extends JTieTestBase {

    public void testDbugUtils() {
        out.println("--> MySqlUtilsTest.testDbugUtils()");


        /*
          "my_thread_end()" should only be called from threads which have
          called "my_thread_init()". loadSystemLibrary() calls
          "my_thread_init()" internally and "my_thread_end()" will be called
          from the static object destructors. So, this function has to be
          called only from the process main thread. This is only critical for
          debug build.
        */
        loadSystemLibrary("ndbclient");

/*
        static native void dbugPush(String state);
        static native void dbugPop();
        static native void dbugSet(String state);
        static native String dbugExplain(ByteBuffer buffer, int length);
        static native void dbugPrint(String keyword, String message);
*/

        final int len = 1024;
        final ByteBuffer bb = ByteBuffer.allocateDirect(len);

        String s;
        final String s0 = "";
        s = Utils.dbugExplain(bb, len);
        assert (s == null || s.equals(s0));

        s = Utils.dbugExplain(null, len);
        assert s == null;

        s = Utils.dbugExplain(bb, 0);
        assert s == null;

        final String s1 = "t";
        Utils.dbugSet(s1);
        s = Utils.dbugExplain(bb, len);
        assert (s == null || s.equals(s1));

        Utils.dbugSet(null);
        s = Utils.dbugExplain(bb, len);
        assert (s == null || s.equals(s1));

        final String s2 = "d,somename:o,/tmp/somepath";
        Utils.dbugPush(s2);
        s = Utils.dbugExplain(bb, len);
        assert (s == null || s.equals(s2));

        Utils.dbugPush(null);
        s = Utils.dbugExplain(bb, len);
        assert (s == null || s.equals(s2));

        final String s3 = "d,a,b,c,x,y,z";
        Utils.dbugPush(s3);
        s = Utils.dbugExplain(bb, len);
        // allow for different order
        assert (s == null
                || (s.length() == s3.length() && s.matches("[" + s3 + "]+")));

        Utils.dbugPop();
        s = Utils.dbugExplain(bb, len);
        assert (s == null || s.equals(s2));

        Utils.dbugPop();
        s = Utils.dbugExplain(bb, len);
        assert (s == null || s.equals(s1));

        Utils.dbugPush(null);
        s = Utils.dbugExplain(bb, len);
        assert (s == null || s.equals(s1));

        Utils.dbugPop();
        s = Utils.dbugExplain(bb, len);
        assert (s == null || s.equals(s0));

        out.println();
        out.println("<-- MySqlUtilsTest.testDbugUtils()");
    };

    // ----------------------------------------------------------------------

    static public ByteBuffer char2bb(char[] c) {
        int len = c.length;
        ByteBuffer bb = ByteBuffer.allocateDirect(len);
        for(int i = 0 ; i < len ; i++)
            bb.put((byte) c[i]);
        bb.rewind();
        return bb;
    }

    static String bbdump(ByteBuffer sbb) {
        ByteBuffer bb = sbb.asReadOnlyBuffer();
        byte[] bytes = new byte[bb.capacity()];
        bb.get(bytes);
        bb.rewind();
        return Arrays.toString(bytes);
    }

    static int bbcmp(ByteBuffer sbb1, ByteBuffer sbb2) {
        ByteBuffer bb1 = sbb1.asReadOnlyBuffer();
        ByteBuffer bb2 = sbb2.asReadOnlyBuffer();
        Byte b1, b2;
        do {
            b1 = bb1.get();
            b2 = bb2.get();
            if(b1 > b2) return 1;
            if(b1 < b2) return -1;
        } while((b1 != 0) && (b2 != 0));

        return 0;
    }

    static int bbncmp(ByteBuffer sbb1, ByteBuffer sbb2, int n) {
        ByteBuffer bb1 = sbb1.asReadOnlyBuffer();
        ByteBuffer bb2 = sbb2.asReadOnlyBuffer();
        Byte b1, b2;
        int i = 0;
        do {
            b1 = bb1.get();
            b2 = bb2.get();
            if(b1 > b2) return 1;
            if(b1 < b2) return -1;
        } while (i++ < n);

        return 0;
    }

    public void printRecodeResult(int rcode, int lengths[], ByteBuffer b1,
                                  ByteBuffer b2)
    {
        out.println("    Return code: " + rcode + " Len0: "
                    + lengths[0] + " Len1: " + lengths[1]  + "\n"
                    + "    " + bbdump(b1) + " => " + bbdump(b2)
                    );
    }


    public void testCharsetMap() {
        int latin1_num, utf8_num, utf16_num;
        out.println("--> MySqlUtilsTest.testCharsetMap()");

        /*
          "my_thread_end()" should only be called from threads which have
          called "my_thread_init()". loadSystemLibrary() calls
          "my_thread_init()" internally and "my_thread_end()" will be called
          from the static object destructors. So, this function has to be
          called only from the process main thread. This is only critical for
          debug build.
        */
        loadSystemLibrary("ndbclient");
        CharsetMap csmap = CharsetMap.create();

        out.println("--> Test that mysql includes UTF-8 and 16-bit Unicode");
        utf8_num = csmap.getUTF8CharsetNumber();
        utf16_num = csmap.getUTF16CharsetNumber();
        out.println("      UTF-8 charset num: " +  utf8_num +
                    "    UTF-16 or UCS-2 charset num: " +  utf16_num);
        assert (utf8_num != 0);
        assert (utf16_num != 0);
        out.println("<-- Test that mysql includes UTF-8 and 16-bit Unicode");


        out.println("--> Test CharsetMap::getName()");
        String utf8_name = csmap.getName(utf8_num);
        String utf16 = csmap.getMysqlName(csmap.getUTF16CharsetNumber());
        assert (utf8_name.compareTo("UTF-8") == 0);
        /* MySQL 5.1 and earlier will have UCS-2 but later versions may have true
         UTF-16.  For information, print whether UTF-16 or UCS-2 is being used. */
        out.println("      Using mysql \"" + utf16 + "\" for UTF-16.");
        out.println("<-- Test CharsetMap::getName()");

        /* Now we're going to recode.
         We test with a string that begins with the character
         LATIN SMALL LETTER U WITH DIARESIS - unicode code point U+00FC.
         In the latin1 encoding this is a literal 0xFC,
         but in the UTF-8 representation it is 0xC3 0xBC.
         */

        final char[] cmy_word_latin1    = new char[] { 0xFC, 'l', 'k', 'e', 'r', 0 };
        final char[] cmy_word_utf8      = new char[] { 0xC3, 0xBC, 'l', 'k', 'e', 'r', 0 };
        final char[] cmy_word_truncated = new char[] { 0xC3, 0xBC, 'l', 'k', 0 };
        final char[] cmy_bad_utf8       = new char[] { 'l' , 0xBC, 'a', 'd', 0 };


        out.println("--> CharsetMap::recode() Tests");

        {
            ByteBuffer my_word_latin1    = char2bb(cmy_word_latin1);
            ByteBuffer my_word_utf8      = char2bb(cmy_word_utf8);
            out.println("--> Test that latin1 is available.");
            latin1_num = csmap.getCharsetNumber("latin1");
            out.println("    latin1 charset number: " + latin1_num +
                        " standard name: " + csmap.getName(latin1_num));
            assert (latin1_num != 0);
            assert (csmap.getName(latin1_num).compareTo("windows-1252") == 0);
            out.println("    Latin1 source string: " + bbdump(my_word_latin1) + "\n" +
                        "    UTF8 source string:   " + bbdump(my_word_utf8));
            out.println("<-- Test that latin1 is available.");
        }

        {
            out.println("--> RECODE TEST 1: recode from UTF-8 to Latin 1");
            ByteBuffer my_word_utf8      = char2bb(cmy_word_utf8);
            ByteBuffer result_buff       = ByteBuffer.allocateDirect(16);
            int[] lengths = new int[]  { 7 , 16 };

            int rr1 = csmap.recode(lengths, utf8_num, latin1_num,
                                   my_word_utf8, result_buff);
            printRecodeResult(rr1, lengths, my_word_utf8, result_buff);
            assert (rr1 == CharsetMapConst.RecodeStatus.RECODE_OK);
            assert (lengths[0] == 7);
            assert (lengths[1] == 6);
            assert (bbcmp(char2bb(cmy_word_latin1), result_buff) == 0);
            out.println("<-- RECODE TEST 1");
        }

        {
            out.println("--> RECODE TEST 2: recode from Latin1 to to UTF-8");
            ByteBuffer my_word_latin1    = char2bb(cmy_word_latin1);
            ByteBuffer result_buff       = ByteBuffer.allocateDirect(16);
            int[] lengths = new int[]  { 6 , 16 };

            int rr2 = csmap.recode(lengths, latin1_num, utf8_num,
                                   my_word_latin1, result_buff);
            printRecodeResult(rr2, lengths, my_word_latin1, result_buff);
            assert (rr2 == CharsetMapConst.RecodeStatus.RECODE_OK);
            assert (lengths[0] == 6);
            assert (lengths[1] == 7);
            assert (bbcmp(result_buff, char2bb(cmy_word_utf8)) == 0);
            out.println("<-- RECODE TEST 2");
        }

        {
            out.println("--> RECODE TEST 3: too-small result buffer");
            ByteBuffer my_word_latin1    = char2bb(cmy_word_latin1);
            ByteBuffer result_buff       = ByteBuffer.allocateDirect(16);
            ByteBuffer my_word_truncated = char2bb(cmy_word_truncated);
            int[] lengths = new int[]  { 6 , 4 };   // 4 is too small

            int rr3 = csmap.recode(lengths, latin1_num, utf8_num,
                               my_word_latin1, result_buff);
            printRecodeResult(rr3, lengths, my_word_latin1, result_buff);
            assert (rr3 == CharsetMapConst.RecodeStatus.RECODE_BUFF_TOO_SMALL);
            assert (lengths[0] == 3);
            assert (lengths[1] == 4);
            /* Confirm that the first four characters were indeed recoded: */
            assert (bbncmp(result_buff, char2bb(cmy_word_truncated), 4) == 0);
            out.println("<-- RECODE TEST 3");
        }

        {
            out.println("--> RECODE TEST 4: invalid character set");
            ByteBuffer my_word_latin1    = char2bb(cmy_word_latin1);
            ByteBuffer result_buff       = ByteBuffer.allocateDirect(16);
            int[] lengths = new int[]  { 6 , 16 };
            int rr4 = csmap.recode(lengths, 0, 999, my_word_latin1, result_buff);
            out.println("    Return code: " + rr4);
            assert (rr4 == CharsetMapConst.RecodeStatus.RECODE_BAD_CHARSET);
            out.println("<-- RECODE TEST 4");
        }

        {
            out.println("--> RECODE TEST 5: source string is ill-formed UTF-8");
            ByteBuffer my_bad_utf8       = char2bb(cmy_bad_utf8);
            ByteBuffer result_buff       = ByteBuffer.allocateDirect(16);
            int[] lengths = new int[]  { 5 , 16 };
            int rr5 = csmap.recode(lengths, utf8_num, latin1_num,
                                   my_bad_utf8, result_buff);
            out.println("    Return code: " + rr5);
            assert (rr5 == CharsetMapConst.RecodeStatus.RECODE_BAD_SRC);
            out.println("<-- RECODE TEST 5");
        }

        {
            out.println("--> RECODE TEST 6: convert an actual java string to UTF-8");
            // Load the string into a ByteBuffer
            ByteBuffer str_bb = ByteBuffer.allocateDirect(16);
            CharBuffer cb = str_bb.asCharBuffer();
            cb.append("\u00FClker");
            cb.rewind();
            ByteBuffer result_buff       = ByteBuffer.allocateDirect(16);
            int[] lengths = new int[]  { 12 , 16 };

            int rr6 = csmap.recode(lengths, utf16_num, utf8_num,
                                   str_bb, result_buff);
            printRecodeResult(rr6, lengths, str_bb, result_buff);
            assert (lengths[0]) == 12;
            assert (lengths[1]) == 7;
            assert (bbncmp(result_buff, char2bb(cmy_word_utf8), 6) == 0);
            out.println("<-- RECODE TEST 6");
        }

        out.println();

        {
            out.println("--> IS MULTIBYTE TEST");
            boolean[] result = csmap.isMultibyte(latin1_num);
            assert (!result[0]);
            result = csmap.isMultibyte(utf16_num);
            assert (result[0]);
            result = csmap.isMultibyte(utf8_num);
            assert (result[0]);
            int nNull = 0, nSingle = 0, nMulti = 0;
            for(int i = 0; i < 256 ; i++) {
              result = csmap.isMultibyte(i);
              if(result == null) nNull++;
              else {
                if(result[0]) nMulti++;
                else nSingle++;
              }
            }
            out.println("    Unused: " + nNull +
                        " single-byte: " +nSingle + " multi-byte: " + nMulti  );

            assert (nNull > 0);
            assert (nSingle > 0);
            assert (nMulti > 0);
            out.println("<-- IS MULTIBYTE TEST");
        }
        out.println("<-- MySqlUtilsTest.testCharsetMap()");
    };

    // ----------------------------------------------------------------------

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

    public void testDecimalConv() {
        out.println("--> MySqlUtilsTest.testDecimalConv()");


        /*
          "my_thread_end()" should only be called from threads which have
          called "my_thread_init()". loadSystemLibrary() calls
          "my_thread_init()" internally and "my_thread_end()" will be called
          from the static object destructors. So, this function has to be
          called only from the process main thread. This is only critical for
          debug build.
        */
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
        out.println("<-- MySqlUtilsTest.testDecimalConv()");
    };

    // ----------------------------------------------------------------------

    public void test() {
        out.println("--> MySqlUtilsTest.test()");

        testDbugUtils();
        testCharsetMap();
        testDecimalConv();

        out.println();
        out.println("<-- MySqlUtilsTest.test()");
    };

    static public void main(String[] args) throws Exception {
        out.println("--> MySqlUtilsTest.main()");

        out.println();
        MySqlUtilsTest test = new MySqlUtilsTest();
        test.test();

        out.println();
        out.println("<-- MySqlUtilsTest.main()");
    }
}
