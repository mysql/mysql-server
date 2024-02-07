/* Copyright (c) 2003, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include "md5_hash.hpp"
#include "my_config.h"

#include <my_compiler.h>  // likely
#include <my_config.h>    // big/little endian
#include <string.h>       // memcpy()

/*
 * This code implements the MD5 message-digest algorithm.
 * The algorithm is due to Ron Rivest.  This code was
 * written by Colin Plumb in 1993, no copyright is claimed.
 * This code is in the public domain; do with it what you wish.
 *
 * Equivalent code is available from RSA Data Security, Inc.
 * This code has been tested against that, and is equivalent,
 * except that you don't need to include two pages of legalese
 * with every copy.
 *
 * The code has been modified by Mikael Ronstroem to handle
 * calculating a hash value of a key that is always a multiple
 * of 4 bytes long. Word 0 of the calculated 4-word hash value
 * is returned as the hash value.
 */

/* The four core functions - F1 is optimized somewhat */

/* #define F1(x, y, z) (x & y | ~x & z) */
#define F1(x, y, z) (z ^ (x & (y ^ z)))
#define F2(x, y, z) F1(z, x, y)
#define F3(x, y, z) (x ^ y ^ z)
#define F4(x, y, z) (y ^ (x | ~z))

/* This is the central step in the MD5 algorithm. */
#define MD5STEP(f, w, x, y, z, data, s) \
  (w += f(x, y, z) + data, w = w << s | w >> (32 - s), w += x)

/*
 * The core of the MD5 algorithm, this alters an existing MD5 hash to
 * reflect the addition of 16 longwords of new data.  MD5Update blocks
 * the data and converts bytes into longwords for this routine.
 */
static void MD5Transform(Uint32 buf[4], Uint32 const in[16]) {
  Uint32 a, b, c, d;

  a = buf[0];
  b = buf[1];
  c = buf[2];
  d = buf[3];

  MD5STEP(F1, a, b, c, d, in[0] + 0xd76aa478, 7);
  MD5STEP(F1, d, a, b, c, in[1] + 0xe8c7b756, 12);
  MD5STEP(F1, c, d, a, b, in[2] + 0x242070db, 17);
  MD5STEP(F1, b, c, d, a, in[3] + 0xc1bdceee, 22);
  MD5STEP(F1, a, b, c, d, in[4] + 0xf57c0faf, 7);
  MD5STEP(F1, d, a, b, c, in[5] + 0x4787c62a, 12);
  MD5STEP(F1, c, d, a, b, in[6] + 0xa8304613, 17);
  MD5STEP(F1, b, c, d, a, in[7] + 0xfd469501, 22);
  MD5STEP(F1, a, b, c, d, in[8] + 0x698098d8, 7);
  MD5STEP(F1, d, a, b, c, in[9] + 0x8b44f7af, 12);
  MD5STEP(F1, c, d, a, b, in[10] + 0xffff5bb1, 17);
  MD5STEP(F1, b, c, d, a, in[11] + 0x895cd7be, 22);
  MD5STEP(F1, a, b, c, d, in[12] + 0x6b901122, 7);
  MD5STEP(F1, d, a, b, c, in[13] + 0xfd987193, 12);
  MD5STEP(F1, c, d, a, b, in[14] + 0xa679438e, 17);
  MD5STEP(F1, b, c, d, a, in[15] + 0x49b40821, 22);

  MD5STEP(F2, a, b, c, d, in[1] + 0xf61e2562, 5);
  MD5STEP(F2, d, a, b, c, in[6] + 0xc040b340, 9);
  MD5STEP(F2, c, d, a, b, in[11] + 0x265e5a51, 14);
  MD5STEP(F2, b, c, d, a, in[0] + 0xe9b6c7aa, 20);
  MD5STEP(F2, a, b, c, d, in[5] + 0xd62f105d, 5);
  MD5STEP(F2, d, a, b, c, in[10] + 0x02441453, 9);
  MD5STEP(F2, c, d, a, b, in[15] + 0xd8a1e681, 14);
  MD5STEP(F2, b, c, d, a, in[4] + 0xe7d3fbc8, 20);
  MD5STEP(F2, a, b, c, d, in[9] + 0x21e1cde6, 5);
  MD5STEP(F2, d, a, b, c, in[14] + 0xc33707d6, 9);
  MD5STEP(F2, c, d, a, b, in[3] + 0xf4d50d87, 14);
  MD5STEP(F2, b, c, d, a, in[8] + 0x455a14ed, 20);
  MD5STEP(F2, a, b, c, d, in[13] + 0xa9e3e905, 5);
  MD5STEP(F2, d, a, b, c, in[2] + 0xfcefa3f8, 9);
  MD5STEP(F2, c, d, a, b, in[7] + 0x676f02d9, 14);
  MD5STEP(F2, b, c, d, a, in[12] + 0x8d2a4c8a, 20);

  MD5STEP(F3, a, b, c, d, in[5] + 0xfffa3942, 4);
  MD5STEP(F3, d, a, b, c, in[8] + 0x8771f681, 11);
  MD5STEP(F3, c, d, a, b, in[11] + 0x6d9d6122, 16);
  MD5STEP(F3, b, c, d, a, in[14] + 0xfde5380c, 23);
  MD5STEP(F3, a, b, c, d, in[1] + 0xa4beea44, 4);
  MD5STEP(F3, d, a, b, c, in[4] + 0x4bdecfa9, 11);
  MD5STEP(F3, c, d, a, b, in[7] + 0xf6bb4b60, 16);
  MD5STEP(F3, b, c, d, a, in[10] + 0xbebfbc70, 23);
  MD5STEP(F3, a, b, c, d, in[13] + 0x289b7ec6, 4);
  MD5STEP(F3, d, a, b, c, in[0] + 0xeaa127fa, 11);
  MD5STEP(F3, c, d, a, b, in[3] + 0xd4ef3085, 16);
  MD5STEP(F3, b, c, d, a, in[6] + 0x04881d05, 23);
  MD5STEP(F3, a, b, c, d, in[9] + 0xd9d4d039, 4);
  MD5STEP(F3, d, a, b, c, in[12] + 0xe6db99e5, 11);
  MD5STEP(F3, c, d, a, b, in[15] + 0x1fa27cf8, 16);
  MD5STEP(F3, b, c, d, a, in[2] + 0xc4ac5665, 23);

  MD5STEP(F4, a, b, c, d, in[0] + 0xf4292244, 6);
  MD5STEP(F4, d, a, b, c, in[7] + 0x432aff97, 10);
  MD5STEP(F4, c, d, a, b, in[14] + 0xab9423a7, 15);
  MD5STEP(F4, b, c, d, a, in[5] + 0xfc93a039, 21);
  MD5STEP(F4, a, b, c, d, in[12] + 0x655b59c3, 6);
  MD5STEP(F4, d, a, b, c, in[3] + 0x8f0ccc92, 10);
  MD5STEP(F4, c, d, a, b, in[10] + 0xffeff47d, 15);
  MD5STEP(F4, b, c, d, a, in[1] + 0x85845dd1, 21);
  MD5STEP(F4, a, b, c, d, in[8] + 0x6fa87e4f, 6);
  MD5STEP(F4, d, a, b, c, in[15] + 0xfe2ce6e0, 10);
  MD5STEP(F4, c, d, a, b, in[6] + 0xa3014314, 15);
  MD5STEP(F4, b, c, d, a, in[13] + 0x4e0811a1, 21);
  MD5STEP(F4, a, b, c, d, in[4] + 0xf7537e82, 6);
  MD5STEP(F4, d, a, b, c, in[11] + 0xbd3af235, 10);
  MD5STEP(F4, c, d, a, b, in[2] + 0x2ad7d2bb, 15);
  MD5STEP(F4, b, c, d, a, in[9] + 0xeb86d391, 21);

  buf[0] += a;
  buf[1] += b;
  buf[2] += c;
  buf[3] += d;
}

/*
 * Start MD5 accumulation. Set bit count to 0 and buffer to mysterious
 * initialization constants.
 */
void md5_hash(Uint32 result[4], const char *keybuf, Uint32 no_of_bytes) {
  Uint32 buf[4];
  union {
    Uint64 transform64_buf[8];
    Uint32 transform32_buf[16];
  };

  // Round up to full word length
  const Uint32 len = (no_of_bytes + 3) & ~3;

  buf[0] = 0x67452301;
  buf[1] = 0xefcdab89;
  buf[2] = 0x98badcfe;
  buf[3] = 0x10325476;

  while (no_of_bytes >= 64) {
    memcpy(transform32_buf, keybuf, 64);
    keybuf += 64;
    no_of_bytes -= 64;
    MD5Transform(buf, transform32_buf);
  }

  if (unlikely(no_of_bytes >= 61)) {  // Will be a full frame when padded
    // Last word written will not be a full word -> zero pad before memcpy over
    transform32_buf[15] = 0;
    memcpy(transform32_buf, keybuf, no_of_bytes);
    keybuf += no_of_bytes;
    no_of_bytes = 0;
    MD5Transform(buf, transform32_buf);
  }

  // Remaining words, including zero padding, to get to a word-aligned size.
  const Uint32 no_of_32_words = (no_of_bytes + 3) >> 2;

  transform64_buf[0] = 0;
  transform64_buf[1] = 0;
  transform64_buf[2] = 0;
  transform64_buf[3] = 0;
  transform64_buf[4] = 0;
  transform64_buf[5] = 0;
  transform64_buf[6] = 0;
  transform64_buf[7] = (Uint64)len;

  // 0x800... Is used as an end / length mark, possibly overwriting the 'len'.
  transform32_buf[no_of_32_words] = 0x80000000;

  if (likely(no_of_32_words > 0)) {
    // Last word written may not be a full word -> zero pad before memcpy over
    transform32_buf[no_of_32_words - 1] = 0;
    memcpy(transform32_buf, keybuf, no_of_bytes);

    if (unlikely(no_of_32_words >= 14)) {
      // On a little endian platform the memcpy() + 0x800.. wrote over 'len'
      // located at 32_buf[14], 32_buf[15] was already zero.
      // On a big endian, len is written to 32_buf[15] and not written over
      // if '#32_words == 14' -> We clear it as it is set in next frame instead.
#ifdef WORDS_BIGENDIAN
      if (no_of_32_words == 14) transform32_buf[15] = 0;
#endif
      // Note: 'len' should have been written to 32_buf[15] for both
      //       big/little endian.
      // For backward bug compatability it is too late to fix it now.
      MD5Transform(buf, transform32_buf);
      transform64_buf[0] = 0;
      transform64_buf[1] = 0;
      transform64_buf[2] = 0;
      transform64_buf[3] = 0;
      transform64_buf[4] = 0;
      transform64_buf[5] = 0;
      transform64_buf[6] = 0;
      transform64_buf[7] = (Uint64)len;
    }
  }
  MD5Transform(buf, transform32_buf);

  result[0] = buf[0];
  result[1] = buf[1];
  result[2] = buf[2];
  result[3] = buf[3];
}

//////////////////////////////////////////////////
//////////////////////////////////////////////////
//////////////////////////////////////////////////
//////////////////////////////////////////////////
//////////////////// Unit test ///////////////////

#ifdef TEST_MD5_HASH

#include <iostream>
#include <util/NdbTap.hpp>

#define MAX_TEST_SAMPLE_WORDS 1024

struct test_sample {
  Uint32 hash_length_bytes;
  Uint32 results[4];
};

/**
 * Our MD5 hash implementation does not produce the same hash results
 * on BIG- vs LITTLE ENDIAN platforms ... unfortunately. As such it is
 * not really a true MD5-hash, just based on the MD5 algorithm.
 * To late to fix that now, so we just need to test that it will
 * forever produce the same results as recorded below.
 */
#ifdef WORDS_BIGENDIAN

// Results recorded with disabled printf() in main, should be stable.
test_sample test_samples[] = {
    // Word aligned length:
    {4, {0XF395424, 0X9415E491, 0X4DABC09B, 0X4A6CC54D}},
    {8, {0X6B9A039D, 0X7764C1C6, 0X81EED20, 0XBC64B5CC}},
    {12, {0XBF169410, 0X7FB3436, 0XE17BA74E, 0XAB0A4067}},
    {16, {0XFB7A7DC0, 0XBACAF813, 0X8D8BC4B1, 0X1452750A}},
    {36, {0XF36CE77A, 0X2B821864, 0X940B1325, 0XA009F3E6}},
    {52, {0XC0E14BD5, 0XE82F5B09, 0XBAE9CF54, 0XCE2BBA77}},  // 13 words
    {56, {0XE5C7B4A7, 0X9F9E8938, 0XB9EAEA5D, 0X425659D7}},  // 14 words
    {60, {0X7F4831AB, 0XA69065B8, 0XCDF3EC3A, 0X60665966}},  // 15 words
    {64, {0XA9218B4E, 0X3CDE1EA6, 0XFC70CC6F, 0X5609446D}},
    {256, {0XD649F171, 0X504369AD, 0XE14ED23, 0X66057D72}},
    {1020, {0X20F7CCC2, 0X8A6198CC, 0X48BF1951, 0XADCB99D7}},
    {1024, {0X29B54318, 0X9EB15B79, 0XC1739255, 0XD75498B7}},

    // Non word-aligned lengths, need zero padding to word-aligned length.
    // Take extra care for odd lengths at end of 64 byte hash frame
    {1, {0X59DCEB96, 0XF8ED063F, 0XE3763E75, 0X87C861D6}},
    {2, {0X4991D477, 0X2366FB09, 0XB583CAA3, 0XDDBB60F6}},
    {3, {0X34FB9C00, 0X8BB6DCD1, 0X5C6BB6A8, 0XCF4239B6}},
    {5, {0X8643880B, 0XA3479A33, 0X73FDCC08, 0X959390FE}},
    {17, {0XC6681D05, 0X55FCE02A, 0X1A6D6FEE, 0XB2EBE50B}},
    {31, {0X2F9529C9, 0X93B9643A, 0X86B2F72A, 0XEA3347DE}},
    {57, {0X31B19798, 0X1E83721D, 0X30305C46, 0X8AEEC7F3}},
    {58, {0X9284EC0A, 0X827CD053, 0X950A755E, 0XED255411}},
    {59, {0X305A324, 0XE6B6316A, 0X83986039, 0XBED3699}},
    {61, {0X25FFDB98, 0XBED3B17C, 0X9A45F986, 0X961EAD82}},
    {62, {0X684239B0, 0X84B9D739, 0X2C5DBBCC, 0X53A3E523}},
    {63, {0X36588F97, 0XFA176522, 0XEBAFC1F3, 0XDF01440A}},
    {65, {0XA7F1BE6B, 0XB0CC6470, 0X16D85E5, 0XA8FD5C0}},
    {255, {0X24C328DC, 0X1C73CC4C, 0XB9945B3D, 0X984AE8BB}},
    {257, {0X1390F2B6, 0X1A790B7F, 0X156978E9, 0XD774F0A3}},

    // 256 + [57..63]
    {313, {0XECFE1103, 0X36F28DA, 0XD0D974F9, 0X4AA55D8B}},
    {314, {0XB87F6F4E, 0XADE849F6, 0X2B2EC1F8, 0XA7F731D1}},
    {315, {0X891D3DD5, 0XB3EFDFFD, 0XDA794148, 0XF653A042}},
    {317, {0X61F6D3EB, 0X5DDBD222, 0XD7288532, 0X440E8DC0}},
    {318, {0XD5FFEB33, 0XD74C979D, 0X33104AB, 0XBC81DCFA}},
    {319, {0X13A211D0, 0X9F66713, 0X3FCBB781, 0X907B1108}},

    {414, {0XDED72A4A, 0XB241C3A3, 0XA018DFAE, 0X2A494218}},

    {0, {0, 0, 0, 0}}  // End mark
};

#else
// Results recorded with disabled printf() in main, should be stable.
test_sample test_samples[] = {
    // Word aligned length:
    {4, {0X44A5CFC1, 0XD0A9457A, 0XD2FB5247, 0XF98C9442}},
    {8, {0X61101D91, 0XF4FB177F, 0X949C004, 0XAB9A0B85}},
    {12, {0XE562568B, 0XCA97F111, 0X1564B44C, 0XB14E176D}},
    {16, {0XFCFD8C82, 0XB1D675AE, 0X5F3AEF04, 0X90213611}},
    {36, {0X39FBD1C5, 0XFCDA295F, 0X2FED843, 0X48DD2822}},
    {52, {0XD3DA70FF, 0XA8B89B73, 0XDAD727A2, 0XC6F092A0}},  // 13 words
    {56, {0XB8CDB467, 0XCBD7A0A8, 0X4E308B78, 0X538CE38}},   // 14 words
    {60, {0X5D98F56F, 0X8FB7CAB8, 0X21CF5713, 0X116FA059}},  // 15 words
    {64, {0X9C292B7F, 0X41E67FEC, 0X249F6124, 0X46AA735A}},
    {256, {0X2F6B1711, 0XAFE7EAC7, 0X57CA56CE, 0X5BA88E99}},
    {1020, {0XF1D20AEC, 0X9B92D50F, 0X735B7161, 0X6F11F158}},
    {1024, {0XE5BBA072, 0XE4BE56DE, 0XB1393C17, 0X6EFD7715}},

    // Non word-aligned lengths, need zero padding to word-aligned length
    // Take extra care for odd lengths at end of 64 byte hash frame
    {1, {0X7658F3F0, 0XC3F148B5, 0XF3104528, 0XD95313D6}},
    {2, {0XA2A47648, 0X7855531C, 0X51A567E8, 0XADB51801}},
    {3, {0X8DAA4E4B, 0XD9875900, 0X116180BB, 0X68B361BF}},
    {5, {0X80DD1E79, 0X29D2510D, 0X63AAE629, 0XD28B2B57}},
    {17, {0X9C4DB17B, 0XCA3D02BD, 0X9337CBA1, 0XB63F384F}},
    {31, {0X7CF12E95, 0X67EE130D, 0XBA068B80, 0XA362092}},
    {57, {0XEF453902, 0X36D545E9, 0X6B0586AA, 0XA5FE9C31}},
    {58, {0X8D7F7B18, 0X88170337, 0XFA0A855D, 0X3611DF60}},
    {59, {0X3B316EB1, 0XBE5AE4F7, 0X52F618B9, 0XC22D2BF6}},
    {61, {0X1FD7D9C6, 0XB0AAC28C, 0XCFE04381, 0X1E888A96}},
    {62, {0X4E1F2B4F, 0XA4AF3D2, 0X7292C6D2, 0X54C17201}},
    {63, {0X3F809A9F, 0XEB450EF6, 0XFAF82F64, 0X9E1A544E}},
    {65, {0XCE9129DE, 0X334915EA, 0XAB120798, 0X7BF3B391}},
    {255, {0XAC3A0DF, 0X1F3F9DCF, 0XC62C469C, 0X3ABA904C}},
    {257, {0X80D41442, 0XED8555EF, 0XC9BE148, 0X68A234A1}},

    // 256 + [57..63]
    {313, {0X9D9E8E55, 0XE271C692, 0XA1D293D5, 0XEA676EB9}},
    {314, {0X1966036A, 0XF8A9876F, 0X9050ABAE, 0X79E298D5}},
    {315, {0X94E648C5, 0X68A39EB1, 0X8C6B96E5, 0X326C577F}},
    {317, {0XCD41E355, 0X6A9F0DC6, 0XE385E46F, 0X74772010}},
    {318, {0X67569796, 0X25CE77AE, 0X3FC54600, 0X73658729}},
    {319, {0XCAACA43B, 0XF4D943D6, 0X80977D58, 0X80A867D6}},

    {414, {0X4375D8C9, 0X293A308B, 0XE8833025, 0X7C97AC21}},

    {0, {0, 0, 0, 0}}  // End mark
};
#endif

int main() {
  Uint32 test_data[MAX_TEST_SAMPLE_WORDS];

  // Generate some deterministic test samples:
  for (int i = 0; i < MAX_TEST_SAMPLE_WORDS; i++) {
    test_data[i] = 0xcafebabe + i;
  }

  plan(34 + 128);  // Number of tests

  // Test against prerecorded test_samples[] from 'old' hash algorithm.
  // Intention is to verify that we are still backwards bug-compatible
  // with all the shortcommings in our md5-like-implementations.
  for (test_sample *sample = test_samples; sample->hash_length_bytes > 0;
       sample++) {
    Uint32 results[4];
    md5_hash(results, (char *)test_data, sample->hash_length_bytes);
    ok(memcmp(results, sample->results, sizeof(results)) == 0,
       "Hashed bytes:%u", sample->hash_length_bytes);
  }

  // Test zero-padded, vs non-padded & non-wordlength-aligned data
  // for (Uint32 len=0; len<MAX_TEST_SAMPLE_WORDS*4; len++) {
  for (Uint32 len = 0; len < 128; len++) {  // Test 2 hash frames
    char buffer[MAX_TEST_SAMPLE_WORDS * 4];

    // We copy the full test_data[] to 'scramble' if not correctly padded
    memcpy(buffer, test_data, MAX_TEST_SAMPLE_WORDS * 4);
    Uint32 aligned_len = len;
    while ((aligned_len & 3) != 0) {
      buffer[aligned_len++] = 0;
    }

    // md5_hash should give the same result for non-zero-padded vs padded
    Uint32 results1[4], results2[4];
    md5_hash(results1, (char *)test_data, len);
    md5_hash(results2, buffer, aligned_len);
    ok(memcmp(results1, results2, sizeof(results1)) == 0, "Hashed bytes:%u",
       len);
  }

  return exit_status();
}

#endif
