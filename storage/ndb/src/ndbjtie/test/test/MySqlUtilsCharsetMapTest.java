/*
 Copyright (c) 2010, 2012, Oracle and/or its affiliates. All rights reserved.

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
 * MySqlUtilsCharsetMapTest.java
 */

package test;

import java.nio.ByteBuffer;
import java.nio.CharBuffer;
import java.nio.charset.Charset;
import java.util.Arrays;

import com.mysql.ndbjtie.mysql.CharsetMap;
import com.mysql.ndbjtie.mysql.CharsetMapConst;

/**
 * Tests the basic functioning of the NdbJTie libary: mysql CharsetMap utils
 * for character set conversions.
 */
public class MySqlUtilsCharsetMapTest extends JTieTestBase {

    static public ByteBuffer char2bb(char[] c) {        
        int len = c.length;
        ByteBuffer bb = ByteBuffer.allocateDirect(len);
        for(int i = 0 ; i < len ; i++) 
            bb.put((byte) c[i]);        
        bb.rewind();
        return bb;
    }

    String bbdump (ByteBuffer sbb) {
        ByteBuffer bb = sbb.asReadOnlyBuffer();
        byte[] bytes = new byte[bb.capacity()];
        bb.get(bytes);
        bb.rewind();
        return Arrays.toString(bytes);
    }
    
    
    int bbcmp(ByteBuffer sbb1, ByteBuffer sbb2) {
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
    
    int bbncmp(ByteBuffer sbb1, ByteBuffer sbb2, int n) {
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
        out.println("         Return code: " + rcode + " Len0: "
                    + lengths[0] + " Len1: " + lengths[1]  + "\n"
                    + "         " + bbdump(b1) + " => " + bbdump(b2)
                    );
    }
        
    
    public void test() {
        int latin1_num, utf8_num, utf16_num;
        out.println("--> MySqlUtilsCharsetMapTest.test()");

        // load native library
        loadSystemLibrary("ndbclient");
        CharsetMap csmap = CharsetMap.create();
                        
        out.println("  --> Test that mysql includes UTF-8 and 16-bit Unicode");
        utf8_num = csmap.getUTF8CharsetNumber();
        utf16_num = csmap.getUTF16CharsetNumber();
        out.println("       UTF-8 charset num: " +  utf8_num + 
                    "     UTF-16 or UCS-2 charset num: " +  utf16_num);
        assert( ! ((utf8_num == 0) || (utf16_num == 0)));
        out.println("  <-- Test that mysql includes UTF-8 and 16-bit Unicode");
        
        
        out.println("  --> Test CharsetMap::getName()");
        String utf8_name = csmap.getName(utf8_num);
        String utf16 = csmap.getMysqlName(csmap.getUTF16CharsetNumber());
        assert(utf8_name.compareTo("UTF-8") == 0);
        /* MySQL 5.1 and earlier will have UCS-2 but later versions may have true
         UTF-16.  For information, print whether UTF-16 or UCS-2 is being used. */
        out.println("       Using mysql \"" + utf16 + "\" for UTF-16.");
        out.println("  <-- Test CharsetMap::getName()");
        
        /* Now we're going to recode. 
         We test with the string "ülker", which begins with the character
         LATIN SMALL LETTER U WITH DIARESIS - unicode code point U+00FC.
         In the latin1 encoding this is a literal 0xFC,
         but in the UTF-8 representation it is 0xC3 0xBC.
         */
        
        final char[] cmy_word_latin1    = new char[] { 0xFC, 'l', 'k', 'e', 'r', 0 }; 
        final char[] cmy_word_utf8      = new char[] { 0xC3, 0xBC, 'l', 'k', 'e', 'r', 0 }; 
        final char[] cmy_word_truncated = new char[] { 0xC3, 0xBC, 'l', 'k', 0 };
        final char[] cmy_bad_utf8       = new char[] { 'l' , 0xBC, 'a', 'd', 0 };
        
   
        out.println("  --> CharsetMap::recode() Tests");

        { 
            ByteBuffer my_word_latin1    = char2bb(cmy_word_latin1);
            ByteBuffer my_word_utf8      = char2bb(cmy_word_utf8);
            out.println("     --> Test that latin1 is available.");
            latin1_num = csmap.getCharsetNumber("latin1");    
            out.println("         latin1 charset number: " + latin1_num + 
                        " standard name: " + csmap.getName(latin1_num));
            assert(latin1_num != 0);
            assert(csmap.getName(latin1_num).compareTo("windows-1252") == 0);        
            out.println("         Latin1 source string: " + bbdump(my_word_latin1) + "\n" +
                        "         UTF8 source string:   " + bbdump(my_word_utf8));
            out.println("     <-- Test that latin1 is available.");
        }
        
        {
            out.println("     --> RECODE TEST 1: recode from UTF-8 to Latin 1");
            ByteBuffer my_word_utf8      = char2bb(cmy_word_utf8);
            ByteBuffer result_buff       = ByteBuffer.allocateDirect(16);
            int[] lengths = new int[]  { 7 , 16 };
            
            int rr1 = csmap.recode(lengths, utf8_num, latin1_num, 
                                   my_word_utf8, result_buff);
            printRecodeResult(rr1, lengths, my_word_utf8, result_buff);
            assert(rr1 == CharsetMapConst.RecodeStatus.RECODE_OK);
            assert(lengths[0] == 7);
            assert(lengths[1] == 6);
            assert(bbcmp(char2bb(cmy_word_latin1), result_buff) == 0);
            out.println("     <-- RECODE TEST 1");
        }        
        
        {
            out.println("     --> RECODE TEST 2: recode from Latin1 to to UTF-8");
            ByteBuffer my_word_latin1    = char2bb(cmy_word_latin1);
            ByteBuffer result_buff       = ByteBuffer.allocateDirect(16);
            int[] lengths = new int[]  { 6 , 16 };

            int rr2 = csmap.recode(lengths, latin1_num, utf8_num,
                                   my_word_latin1, result_buff);
            printRecodeResult(rr2, lengths, my_word_latin1, result_buff);
            assert(rr2 == CharsetMapConst.RecodeStatus.RECODE_OK);
            assert(lengths[0] == 6);
            assert(lengths[1] == 7);
            assert(bbcmp(result_buff, char2bb(cmy_word_utf8)) == 0);
            out.println("     <-- RECODE TEST 2");
        }

        {
            out.println("     --> RECODE TEST 3: too-small result buffer");
            ByteBuffer my_word_latin1    = char2bb(cmy_word_latin1);
            ByteBuffer result_buff       = ByteBuffer.allocateDirect(16);
            ByteBuffer my_word_truncated = char2bb(cmy_word_truncated);
            int[] lengths = new int[]  { 6 , 4 };   // 4 is too small
  
            int rr3 = csmap.recode(lengths, latin1_num, utf8_num,
                               my_word_latin1, result_buff);
            printRecodeResult(rr3, lengths, my_word_latin1, result_buff);
            assert(rr3 == CharsetMapConst.RecodeStatus.RECODE_BUFF_TOO_SMALL);
            assert(lengths[0] == 3);
            assert(lengths[1] == 4);
            /* Confirm that the first four characters were indeed recoded: */
            assert(bbncmp(result_buff, char2bb(cmy_word_truncated), 4) == 0);
            out.println("     <-- RECODE TEST 3");
        }

        {
            out.println("     --> RECODE TEST 4: invalid character set");
            ByteBuffer my_word_latin1    = char2bb(cmy_word_latin1);
            ByteBuffer result_buff       = ByteBuffer.allocateDirect(16);
            int[] lengths = new int[]  { 6 , 16 };           
            int rr4 = csmap.recode(lengths, 0, 999, my_word_latin1, result_buff);
            out.println("          Return code: " + rr4);
            assert(rr4 == CharsetMapConst.RecodeStatus.RECODE_BAD_CHARSET);
            out.println("     <-- RECODE TEST 4");
        }
        
        {
            out.println("     --> RECODE TEST 5: source string is ill-formed UTF-8");
            ByteBuffer my_bad_utf8       = char2bb(cmy_bad_utf8);
            ByteBuffer result_buff       = ByteBuffer.allocateDirect(16);
            int[] lengths = new int[]  { 5 , 16 };          
            int rr5 = csmap.recode(lengths, utf8_num, latin1_num, 
                                   my_bad_utf8, result_buff);
            out.println("          Return code: " + rr5);
            assert(rr5 == CharsetMapConst.RecodeStatus.RECODE_BAD_SRC);
            out.println("     <-- RECODE TEST 5");
        }
        
        {
            out.println("     --> RECODE TEST 6: convert an actual java string to UTF-8");
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
            assert(lengths[0]) == 12;
            assert(lengths[1]) == 7;
            assert(bbncmp(result_buff, char2bb(cmy_word_utf8), 6) == 0);        
            out.println("     <-- RECODE TEST 6");
        }
        
        out.println();
        
        {
            out.println("     --> IS MULTIBYTE TEST");
            boolean[] result = csmap.isMultibyte(latin1_num);
            assert(!result[0]);
            result = csmap.isMultibyte(utf16_num);
            assert(result[0]);
            result = csmap.isMultibyte(utf8_num);
            assert(result[0]);
            int nNull = 0, nSingle = 0, nMulti = 0;
            for(int i = 0; i < 256 ; i++) {
              result = csmap.isMultibyte(i);
              if(result == null) nNull++;
              else {
                if(result[0]) nMulti++;
                else nSingle++;
              }
            }
            out.println("          Unused: " + nNull + 
                        " single-byte: " +nSingle + " multi-byte: " + nMulti  );

            assert(nNull > 0);
            assert(nSingle > 0);
            assert(nMulti > 0);
            out.println("     <-- IS MULTIBYTE TEST");            
        }        
        out.println("<-- MySqlUtilsCharsetMapTest.test()");
    };


    static public void main(String[] args) throws Exception {
        out.println("--> MySqlUtilsCharsetMapTest.main()");

        out.println();
        MySqlUtilsCharsetMapTest test = new MySqlUtilsCharsetMapTest();
        test.test();

        out.println();
        out.println("<-- MySqlUtilsCharsetMapTest.main()");
    }
}
