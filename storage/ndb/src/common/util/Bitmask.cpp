/*
   Copyright (c) 2004, 2022, Oracle and/or its affiliates.

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

#include "util/Bitmask.hpp"

void
BitmaskImpl::getFieldImpl(const Uint32 src[],
			  unsigned shiftL, unsigned len, Uint32 dst[])
{
  /* Copy whole words of src to dst, shifting src left
   * by shiftL.  Undefined bits of the last written dst word
   * should be zeroed.
   */
  assert(shiftL < 32);

  unsigned shiftR = 32 - shiftL;
  unsigned undefined = shiftL ? ~0 : 0;

  /* Merge first word with previously set bits if there's a shift */
  * dst = shiftL ? * dst : 0;

  /* Treat the zero-shift case separately to avoid
   * trampling or reading past the end of src
   */
  if (shiftL == 0)
  {
    while(len >= 32)
    {
      * dst++ = * src++;
      len -=32;
    }

    if (len != 0)
    {
      /* Last word has some bits set */
      Uint32 mask= ((1 << len) -1); // 0000111
      * dst = (* src) & mask;
    }
  }
  else // shiftL !=0, need to build each word from two words shifted
  {
    while(len >= 32)
    {
      * dst++ |= (* src) << shiftL;
      * dst = ((* src++) >> shiftR) & undefined;
      len -= 32;
    }

    /* Have space for shiftR more bits in the current dst word
     * is that enough?
     */
    if(len <= shiftR)
    {
      /* Fit the remaining bits in the current dst word */
      * dst |= ((* src) & ((1 << len) - 1)) << shiftL;
    }
    else
    {
      /* Need to write to two dst words */
      * dst++ |= ((* src) << shiftL);
      * dst = ((* src) >> shiftR) & ((1 << (len - shiftR)) - 1) & undefined;
    }
  }
}

void
BitmaskImpl::setFieldImpl(Uint32 dst[],
			  unsigned shiftL, unsigned len, const Uint32 src[])
{
  /**
   *
   * abcd ef00
   * 00ab cdef
   */
  assert(shiftL < 32);
  unsigned shiftR = 32 - shiftL;
  unsigned undefined = shiftL ? ~0 : 0;  
  while(len >= 32)
  {
    * dst = (* src++) >> shiftL;
    * dst++ |= ((* src) << shiftR) & undefined;
    len -= 32;
  }
  
  /* Copy last bits */
  Uint32 mask = ((1 << len) -1);
  * dst = (* dst & ~mask);
  if(len <= shiftR)
  {
    /* Remaining bits fit in current word */
    * dst |= ((* src++) >> shiftL) & mask;
  }
  else
  {
    /* Remaining bits update 2 words */
    * dst |= ((* src++) >> shiftL);
    * dst |= ((* src) & ((1 << (len - shiftR)) - 1)) << shiftR ;
  }
}

/* Bitmask testcase code moved from here to
 * storage/ndb/test/ndbapi/testBitfield.cpp
 * to get coverage from automated testing
 */

template struct BitmaskPOD<1>;
template struct BitmaskPOD<2>; // NdbNodeBitmask
template struct BitmaskPOD<8>; // NodeBitmask
template struct BitmaskPOD<10>;// TrpBitmask
template struct BitmaskPOD<16>;

#ifdef TEST_BITMASK
#include <util/NdbTap.hpp>
#include <util/BaseString.hpp>

#include "parse_mask.hpp"

TAPTEST(Bitmask)
{
    unsigned i, found;
    Bitmask<8> b;
    OK(b.isclear());
    
    unsigned MAX_BITS = 32 * b.Size;
    for (i = 0; i < MAX_BITS; i++)
    {
      if (i > 60)
        continue;
      switch(i)
      {
      case 2:case 3:case 5:case 7:case 11:case 13:case 17:case 19:case 23:
      case 29:case 31:case 37:case 41:case 43:
        break;
      default:;
        b.set(i);
      }
    }
    for (found = i = 0; i < MAX_BITS; i++)
      found+=(int)b.get(i);
    OK(found == b.count());
    OK(found == 47);
    printf("getText: %s\n", BaseString::getText(b).c_str());
    OK(BaseString::getText(b) ==
        "0000000000000000000000000000000000000000000000001ffff5df5f75d753");
    printf("getPrettyText: %s\n", BaseString::getPrettyText(b).c_str());
    OK(BaseString::getPrettyText(b) ==
        "0, 1, 4, 6, 8, 9, 10, 12, 14, 15, 16, 18, 20, 21, 22, 24, 25, 26, "
        "27, 28, 30, 32, 33, 34, 35, 36, 38, 39, 40, 42, 44, 45, 46, 47, "
       "48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59 and 60");
    printf("getPrettyTextShort: %s\n",
           BaseString::getPrettyTextShort(b).c_str());
    OK(BaseString::getPrettyTextShort(b) ==
        "0,1,4,6,8,9,10,12,14,15,16,18,20,21,22,24,25,26,27,28,30,32,"
        "33,34,35,36,38,39,40,42,44,45,46,47,48,49,50,51,52,53,54,55,"
       "56,57,58,59,60");

    // bitNOT Tests
    Bitmask<8> c = b;
    OK(c.equal(b));
    c.bitNOT();
    printf("getPrettyTextShort(c 1): %s\n",
           BaseString::getPrettyTextShort(c).c_str());
    OK(!c.equal(b));
    c.bitNOT();
    OK(c.count() == b.count());
    OK(c.equal(b));
    printf("getPrettyTextShort(c 2): %s\n",
           BaseString::getPrettyTextShort(c).c_str());

    Bitmask<1> d;
    d.set(1);
    d.set(3);
    d.set(4);
    printf("getPrettyTextShort(d 1): %s\n",
           BaseString::getPrettyTextShort(d).c_str());
    OK(d.count() == 3);
    {
      Uint8 tmp[32];
      Uint32 len = d.toArray(tmp, sizeof(tmp));
      printf("toArray(): ");
      for (Uint32 i = 0; i < len; i++)
        printf("%u ", (Uint32)tmp[i]);
      printf("\n");
      OK(len == 3);
      OK(tmp[0] == 1);
      OK(tmp[1] == 3);
      OK(tmp[2] == 4);
    }
    d.bitNOT();
    printf("getPrettyTextShort(d 2): %s\n",
           BaseString::getPrettyTextShort(d).c_str());
    OK(d.get(2));
    OK(!d.get(4));
    OK(d.count() != 3);
    d.bitNOT();
    OK(d.count() == 3);
    printf("getPrettyTextShort(d 3): %s\n",
           BaseString::getPrettyTextShort(d).c_str());

    /*
      parse_mask
    */
    Bitmask<8> mask;
    OK(parse_mask("1,2,5-7,255", mask) == 6);

    {
      Uint8 tmp[8 * 32];
      Uint32 len = mask.toArray(tmp, sizeof(tmp));
      printf("toArray(): ");
      for (Uint32 i = 0; i < len; i++)
        printf("%u ", (Uint32)tmp[i]);
      printf("\n");
      OK(len == 6);
      OK(tmp[0] == 1);
      OK(tmp[1] == 2);
      OK(tmp[2] == 5);
      OK(tmp[3] == 6);
      OK(tmp[4] == 7);
      OK(tmp[5] == 255);
    }

    // Check all specified bits set
    OK(mask.get(1));
    OK(mask.get(2));
    OK(mask.get(5));
    OK(mask.get(6));
    OK(mask.get(7));
    OK(mask.get(255));

    // Check some random bits not set
    OK(!mask.get(0));
    OK(!mask.get(4));
    OK(!mask.get(3));
    OK(!mask.get(8));
    OK(!mask.get(22));
    OK(!mask.get(254));

    // Parse at the limit
    OK(parse_mask("254", mask) == 1);
    OK(parse_mask("255", mask) == 1);

    // Parse invalid spec(s)
    OK(parse_mask("xx", mask) == -1);
    OK(parse_mask("5-", mask) == -1);
    OK(parse_mask("-5", mask) == -1);
    OK(parse_mask("1,-5", mask) == -1);

    // Parse too large spec
    OK(parse_mask("256", mask) == -2);
    OK(parse_mask("1-255,256", mask) == -2);

    // setRange(first, count)
    b.clear();
    b.setRange(1, 14);
    OK(b.count() == 14);
    b.setRange(45, 0);
    OK(b.count() == 14);
    b.setRange(72, 83);
    OK(b.count() == 97);
    b.setRange(250, 6);
    OK(b.get(255));
    OK(b.count() == 103);
    b.setRange(0, 31);
    OK(b.count() == 120);
    b.setRange(32, 32);
    OK(b.count() == 152);
    b.setRange(65, 1);
    OK(b.get(65));
    OK(b.count() == 153);
    b.setRange(7, 0);
    OK(b.count() == 153);
    b.setRange(0, 0);
    OK(b.count() == 153);

    // Check functioning of bitmask "length"

    Bitmask<8> mask_length_test;
    mask_length_test.set((unsigned)0);
    OK(mask_length_test.getPackedLengthInWords() == 1);
    mask_length_test.set(31);
    OK(mask_length_test.getPackedLengthInWords() == 1);
    mask_length_test.set(65);
    mask_length_test.set(1);
    OK(mask_length_test.getPackedLengthInWords() == 3);
    mask_length_test.set(255);
    OK(mask_length_test.getPackedLengthInWords() == 8);
    mask_length_test.clear();
    OK(mask_length_test.getPackedLengthInWords() == 0);

    return 1; // OK
}

#endif

#ifdef BENCH_BITMASK

#include <math.h>
#include <portlib/NdbTick.h>

typedef Uint32 (* FUNC)(Uint32 n);

static
Uint32
fast(Uint32 n)
{
  return BitmaskImpl::count_bits(n) +
    BitmaskImpl::count_bits(n*n);
}

static
Uint32
slow(Uint32 n)
{
  double d = 1 + n;
  double l = log(d);
  double s = sqrt(d);
  double r = d * l * s;
  double t = r > 0 ? r : r < 0 ? -r : 1;
  double u = log(t);
  double v = sqrt(t);
  double w = log(d + s + t + v);
  double x = sqrt(d + s + t + v);
  return (Uint32)(d * l * s * r * t * u * v * w * x);
}

struct Result
{
  Result() { sum = 0; elapsed = 0; }
  Uint32 sum;
  Uint64 elapsed;
};

inline
void
test_empty(Result& res, Uint32 len, unsigned iter, FUNC func)
{
  Uint32 sum = 0;
  Uint64 start = NdbTick_CurrentMillisecond();
  for (Uint32 j = 0; j<iter; j++)
  {
    for (Uint32 k = 0; k<len; k++)
      sum += (* func)(k);
  }
  Uint64 stop = NdbTick_CurrentMillisecond();
  res.sum += sum;
  res.elapsed += (stop - start);
}

template<unsigned sz>
inline
void
test_find(Result& res, const Bitmask<sz> & mask, unsigned iter, FUNC func)
{
  Uint32 sum = 0;
  Uint64 start = NdbTick_CurrentMillisecond();
  for (Uint32 j = 0; j<iter; j++)
  {
    for (Uint32 n = mask.find(0); n != mask.NotFound;
         n = mask.find(n + 1))
    {
      sum += (* func)(n);
    }
  }
  Uint64 stop = NdbTick_CurrentMillisecond();
  res.sum += sum;
  res.elapsed += (stop - start);
}

template<unsigned sz>
inline
void
test_find_fast(Result& res, const Bitmask<sz> & mask, unsigned iter, FUNC func)
{
  Uint32 sum = 0;
  Uint64 start = NdbTick_CurrentMillisecond();
  for (Uint32 j = 0; j<iter; j++)
  {

    for (Uint32 n = BitmaskImpl::find_first(sz, mask.rep.data);
         n != mask.NotFound;
         n = BitmaskImpl::find_next(sz, mask.rep.data, n + 1))
    {
      sum += (* func)(n);
    }
  }
  Uint64 stop = NdbTick_CurrentMillisecond();
  res.sum += sum;
  res.elapsed += (stop - start);
}

template<unsigned sz>
inline
void
test_find_fast_reversed(Result& res, const Bitmask<sz> & mask, unsigned iter, FUNC func)
{
  Uint32 sum = 0;
  Uint64 start = NdbTick_CurrentMillisecond();
  for (Uint32 j = 0; j<iter; j++)
  {

    for (Uint32 n = BitmaskImpl::find_last(sz, mask.rep.data);
         n != mask.NotFound;
         n = BitmaskImpl::find_prev(sz, mask.rep.data, n - 1))
    {
      sum += (* func)(n);
    }
  }
  Uint64 stop = NdbTick_CurrentMillisecond();
  res.sum += sum;
  res.elapsed += (stop - start);
}

template<unsigned sz>
inline
void
test_toArray(Result& res, const Bitmask<sz> & mask, unsigned iter, FUNC func)
{
  Uint32 sum = 0;
  Uint64 start = NdbTick_CurrentMillisecond();
  for (Uint32 j = 0; j<iter; j++)
  {
    Uint8 tmp[256];
    Uint32 cnt = mask.toArray(tmp, sizeof(tmp));
    for (Uint32 n = 0; n<cnt; n++)
      sum += (* func)(tmp[n]);
  }
  Uint64 stop = NdbTick_CurrentMillisecond();
  res.sum += sum;
  res.elapsed += (stop - start);
}

static
Uint64
sub0(Uint64 hi, Uint64 lo)
{
  if (hi > lo)
    return hi - lo;
  return 0;
}

static
Uint64
x_min(Uint64 a, Uint64 b, Uint64 c)
{
  if (a < b && a < c)
    return a;
  if (b < a && b < c)
    return b;
  return c;
}

static
void
do_test(Uint32 len, FUNC func, const char * name, const char * dist)
{
  Uint32 iter = 10000;
  if (func == slow)
    iter = 3000;

  Result res_find, res_fast, res_fast_reversed, res_toArray, res_empty;
  for (Uint32 i = 0; i < (10000 / len); i++)
  {
    Bitmask<8> tmp;
    if (strcmp(dist, "ran") == 0)
    {
      for (Uint32 i = 0; i<len; i++)
      {
        Uint32 b = rand() % (32 * tmp.Size);
        while (tmp.get(b))
        {
          b = rand() % (32 * tmp.Size);
        }
        tmp.set(b);
      }
    }
    else if (strcmp(dist, "low") == 0)
    {
      for (Uint32 i = 0; i<len; i++)
      {
        tmp.set(i);
      }
    }
    test_find(res_find, tmp, iter, func);
    test_find_fast(res_fast, tmp, iter, func);
    test_find_fast_reversed(res_fast_reversed, tmp, iter, func);
    test_toArray(res_toArray, tmp, iter, func);
    test_empty(res_empty, len, iter, func);
  }

  res_find.elapsed = sub0(res_find.elapsed, res_empty.elapsed);
  res_toArray.elapsed = sub0(res_toArray.elapsed, res_empty.elapsed);
  res_fast.elapsed = sub0(res_fast.elapsed, res_empty.elapsed);
  res_fast_reversed.elapsed = sub0(res_fast_reversed.elapsed, res_empty.elapsed);
  Uint64 m = x_min(res_find.elapsed, res_toArray.elapsed, res_fast_reversed.elapsed);
  if (m == 0)
    m = 1;

  Uint64 div = iter * (10000 / len);
  printf("empty(%s,%s, %u)   : %llu ns/iter (elapsed: %llums)\n",
         dist, name, len,
         (1000000 * res_empty.elapsed / div / len),
         res_empty.elapsed);
  printf("find(%s,%s, %u)    : %llu ns/iter (%.3u%%), (sum: %u)\n",
         dist, name, len,
         (1000000 * res_find.elapsed / div),
         Uint32((100 * res_find.elapsed) / m),
         res_find.sum);
  printf("fast(%s,%s, %u)    : %llu ns/iter (%.3u%%), (sum: %u)\n",
         dist, name, len,
         (1000000 * res_fast.elapsed / div),
         Uint32((100 * res_fast.elapsed) / m),
         res_fast.sum);
         printf("toArray(%s,%s, %u) : %llu ns/iter (%.3u%%), (sum: %u)\n",
         dist, name, len,
         (1000000 * res_toArray.elapsed / div),
         Uint32((100 * res_toArray.elapsed) / m),
         res_toArray.sum);
  printf("reversed(%s,%s, %u)    : %llu ns/iter (%.3u%%), (sum: %u)\n",
         dist, name, len,
         (1000000 * res_fast_reversed.elapsed / div),
         Uint32((100 * res_fast_reversed.elapsed) / m),
         res_fast_reversed.sum);
         printf("toArray(%s,%s, %u) : %llu ns/iter (%.3u%%), (sum: %u)\n",
         dist, name, len,
         (1000000 * res_toArray.elapsed / div),
         Uint32((100 * res_toArray.elapsed) / m),
         res_toArray.sum);
  printf("\n");
}

int
main(void)
{
  int pos;
  const int len[] = { 1, 10, 50, 100, 250, 0 };

  pos = 0;
  while (len[pos] != 0)
  {
    Uint32 l = len[pos++];
    do_test(l, slow, "slow", "ran");
  }

  pos = 0;
  while (len[pos] != 0)
  {
    Uint32 l = len[pos++];
    do_test(l, slow, "slow", "low");
  }

  pos = 0;
  while (len[pos] != 0)
  {
    Uint32 l = len[pos++];
    do_test(l, fast, "fast", "ran");
  }
  pos = 0;
  while (len[pos] != 0)
  {
    Uint32 l = len[pos++];
    do_test(l, fast, "fast", "low");
  }

  return 0;
}

#endif
