/* Copyright (C) 2000 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

/*
 * This file is the latin1 character set with German sorting
 *
 * The modern sort order is used, where:
 *
 * 'ä'  ->  "ae"
 * 'ö'  ->  "oe"
 * 'ü'  ->  "ue"
 * 'ß'  ->  "ss"
 */

/*
 * This comment is parsed by configure to create ctype.c,
 * so don't change it unless you know what you are doing.
 *
 * .configure. strxfrm_multiply_latin1_de=2
 */

#include <my_global.h>
#include "m_string.h"
#include "m_ctype.h"

uchar ctype_latin1_de[] = {
    0,
   32, 32, 32, 32, 32, 32, 32, 32, 32, 40, 40, 40, 40, 40, 32, 32,
   32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32,
   72, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
  132,132,132,132,132,132,132,132,132,132, 16, 16, 16, 16, 16, 16,
   16,129,129,129,129,129,129,  1,  1,  1,  1,  1,  1,  1,  1,  1,
    1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1, 16, 16, 16, 16, 16,
   16,130,130,130,130,130,130,  2,  2,  2,  2,  2,  2,  2,  2,  2,
    2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2, 16, 16, 16, 16, 32,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
   72, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
   16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
    1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
    1,  1,  1,  1,  1,  1,  1, 16,  1,  1,  1,  1,  1,  1,  1,  2,
    2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
    2,  2,  2,  2,  2,  2,  2, 16,  2,  2,  2,  2,  2,  2,  2,  2
};

uchar to_lower_latin1_de[] = {
    0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15,
   16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
   32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47,
   48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63,
   64, 97, 98, 99,100,101,102,103,104,105,106,107,108,109,110,111,
  112,113,114,115,116,117,118,119,120,121,122, 91, 92, 93, 94, 95,
   96, 97, 98, 99,100,101,102,103,104,105,106,107,108,109,110,111,
  112,113,114,115,116,117,118,119,120,121,122,123,124,125,126,127,
  128,129,130,131,132,133,134,135,136,137,138,139,140,141,142,143,
  144,145,146,147,148,149,150,151,152,153,154,155,156,157,158,159,
  160,161,162,163,164,165,166,167,168,169,170,171,172,173,174,175,
  176,177,178,179,180,181,182,183,184,185,186,187,188,189,190,191,
  224,225,226,227,228,229,230,231,232,233,234,235,236,237,238,239,
  240,241,242,243,244,245,246,215,248,249,250,251,252,253,254,223,
  224,225,226,227,228,229,230,231,232,233,234,235,236,237,238,239,
  240,241,242,243,244,245,246,247,248,249,250,251,252,253,254,255
};

uchar to_upper_latin1_de[] = {
    0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15,
   16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
   32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47,
   48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63,
   64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79,
   80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95,
   96, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79,
   80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90,123,124,125,126,127,
  128,129,130,131,132,133,134,135,136,137,138,139,140,141,142,143,
  144,145,146,147,148,149,150,151,152,153,154,155,156,157,158,159,
  160,161,162,163,164,165,166,167,168,169,170,171,172,173,174,175,
  176,177,178,179,180,181,182,183,184,185,186,187,188,189,190,191,
  192,193,194,195,196,197,198,199,200,201,202,203,204,205,206,207,
  208,209,210,211,212,213,214,215,216,217,218,219,220,221,222,223,
  192,193,194,195,196,197,198,199,200,201,202,203,204,205,206,207,
  208,209,210,211,212,213,214,247,216,217,218,219,220,221,222,255
};

/*
 * This is a simple latin1 mapping table, which maps all accented
 * characters to their non-accented equivalents.  Note: in this
 * table, 'ä' is mapped to 'A', 'ÿ' is mapped to 'Y', etc. - all
 * accented characters except the following are treated the same way.
 * Ü, ü, Ö, ö, Ä, ä
 */

/* QQ: why it is necessary ? */
uchar sort_order_latin1_de[] = {
    0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15,
   16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
   32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47,
   48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63,
   64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79,
   80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95,
   96, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79,
   80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90,123,124,125,126,127,
  128,129,130,131,132,133,134,135,136,137,138,139,140,141,142,143,
  144,145,146,147,148,149,150,151,152,153,154,155,156,157,158,159,
  160,161,162,163,164,165,166,167,168,169,170,171,172,173,174,175,
  176,177,178,179,180,181,182,183,184,185,186,187,188,189,190,191,
   65, 65, 65, 65,196, 65, 92, 67, 69, 69, 69, 69, 73, 73, 73, 73,
   68, 78, 79, 79, 79, 79,214,215,216, 85, 85, 85,220, 89,222,223,
   65, 65, 65, 65,196, 65, 92, 67, 69, 69, 69, 69, 73, 73, 73, 73,
   68, 78, 79, 79, 79, 79,214,247,216, 85, 85, 85,220, 89,222, 89
};


/* same as sort_order_latin_de, but maps ALL accented chars to unacented ones */
uchar combo1map[]={
    0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15,
   16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
   32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47,
   48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63,
   64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79,
   80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95,
   96, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79,
   80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90,123,124,125,126,127,
  128,129,130,131,132,133,134,135,136,137,138,139,140,141,142,143,
  144,145,146,147,148,149,150,151,152,153,154,155,156,157,158,159,
  160,161,162,163,164,165,166,167,168,169,170,171,172,173,174,175,
  176,177,178,179,180,181,182,183,184,185,186,187,188,189,190,191,
   65, 65, 65, 65, 65, 65, 92, 67, 69, 69, 69, 69, 73, 73, 73, 73,
   68, 78, 79, 79, 79, 79, 79,215,216, 85, 85, 85, 85, 89,222, 83,
   65, 65, 65, 65, 65, 65, 92, 67, 69, 69, 69, 69, 73, 73, 73, 73,
   68, 78, 79, 79, 79, 79, 79,247,216, 85, 85, 85, 85, 89,222, 89
};

uchar combo2map[]={
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,69, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0,69, 0, 0, 0, 0, 0,69, 0, 0,83, 0, 0, 0, 0,69, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,69, 0, 0, 0, 0, 0,69, 0, 0, 0, 0
};

/* no longer needed
#define L1_AE 196
#define L1_ae 228
#define L1_OE 214
#define L1_oe 246
#define L1_UE 220
#define L1_ue 252
#define L1_ss 223
*/

/*
  Some notes about the following comparison rules:
  By definition, my_strnncoll_latin_de must works exactly as if had called
  my_strnxfrm_latin_de() on both strings and compared the result strings.

  This means that:
  Ä must also matches ÁE and Aè, because my_strxn_frm_latin_de() will convert
  both to AE.

  The other option would be to not do any accent removal in
  sort_order_latin_de[] at all
*/

int my_strnncoll_latin1_de(const uchar * s1, int len1,
                           const uchar * s2, int len2)
{
  const uchar *e1 = s1 + len1;
  const uchar *e2 = s2 + len2;
  uchar c1, c12=0, c2, c22=0;

  while ((s1 < e1 || c12) && (s2 < e2 || c22))
  {
    if (c12) { c1=c12; c12=0; } else { c12=combo2map[*s1]; c1=combo1map[*s1++]; }
    if (c22) { c2=c22; c22=0; } else { c22=combo2map[*s2]; c2=combo1map[*s2++]; }
    if (c1 != c2) return (int)c1 - (int)c2;
  }

  /* A simple test of string lengths won't work -- we test to see
   * which string ran out first */
  return (s1 < e1 || c12)? 1 : (s2 < e2 || c22)? -1 : 0;
}


int my_strnxfrm_latin1_de(uchar * dest, const uchar * src, int len, int srclen)
{
  const uchar *dest_orig = dest;
  const uchar *de = dest + len;
  const uchar *se = src + srclen;
  for ( ; src < se && dest < de ; src++)
  {
    uchar chr=combo1map[*src];
    *dest++=chr;
    if ((chr=combo2map[*src]) && dest < de)
      *dest++=chr;
  }
  return dest - dest_orig;
}


int my_strcoll_latin1_de(const uchar * s1, const uchar * s2)
{
  /* XXX QQ: This should be fixed to not call strlen */
  return my_strnncoll_latin1_de(s1, strlen((char*) s1),
				s2, strlen((char*) s2));
}

int my_strxfrm_latin1_de(uchar * dest, const uchar * src, int len)
{
  /* XXX QQ: This should be fixed to not call strlen */
  return my_strnxfrm_latin1_de(dest, src, len, strlen((char*) src));
}

/*
 * Calculate min_str and max_str that ranges a LIKE string.
 * Arguments:
 * ptr		IN: Pointer to LIKE string.
 * ptr_length	IN: Length of LIKE string.
 * escape	IN: Escape character in LIKE.  (Normally '\').
 *		No escape characters should appear in min_str or max_str
 * res_length   IN: Length of min_str and max_str.
 * min_str      IN/OUT: Smallest case sensitive string that ranges LIKE.
 *		Should be space padded to res_length.
 * max_str	IN/OUT: Largest case sensitive string that ranges LIKE.
 *		Normally padded with the biggest character sort value.
 * min_length	OUT: Length of min_str without space padding.
 * max_length	OUT: Length of max_str without space padding.
 *
 * The function should return 0 if ok and 1 if the LIKE string can't be
 * optimized !
 */

#define min_sort_char ((char) 0)
#define max_sort_char ((char) 0xF7)
#define wild_one '_'
#define wild_many '%'

my_bool my_like_range_latin1_de(const char *ptr, uint ptr_length,
				pchar escape, uint res_length,
				char *min_str, char *max_str,
				uint *min_length, uint *max_length)
{
  const char *end = ptr + ptr_length;
  char *min_org = min_str;
  char *min_end = min_str + res_length;

  for (; ptr != end && min_str != min_end; ptr++)
  {
    if (*ptr == escape && ptr + 1 != end)
    {
      ptr++;				/* Skip escape */
      *min_str++ = *max_str++ = *ptr;
      continue;
    }
    if (*ptr == wild_one)		/* '_' in SQL */
    {
      *min_str++ = min_sort_char;
      *max_str++ = max_sort_char;
      continue;
    }
    if (*ptr == wild_many)		/* '%' in SQL */
    {
      *min_length = (uint)(min_str - min_org);
      *max_length = res_length;
      do {
	*min_str++ = ' ';			/* Because if key compression */
	*max_str++ = max_sort_char;
      } while (min_str != min_end);
      return 0;
    }
    *min_str++ = *max_str++ = *ptr;
  }
  *min_length = *max_length = (uint) (min_str - min_org);

  /* Temporary fix for handling wild_one at end of string (key compression) */
  {
    char *tmp;
    for (tmp= min_str ; tmp > min_org && tmp[-1] == '\0';)
      *--tmp=' ';
  }

  while (min_str != min_end)
    *min_str++ = *max_str++ = ' ';		/* Because if key compression */
  return 0;
}
