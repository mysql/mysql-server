/* Copyright (C) 2000 MySQL AB & MySQL Finland AB & TCX DataKonsult AB
   
   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.
   
   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.
   
   You should have received a copy of the GNU Library General Public
   License along with this library; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place - Suite 330, Boston,
   MA 02111-1307, USA */

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

#include <global.h>
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
 * accented characters are treated the same way.
 *
 * SPECIAL NOTE: 'ß' (the sz ligature), which isn't really an
 * accented 's', is mapped to 'S', to simplify the sorting
 * functions.
 */
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
   65, 65, 65, 65, 65, 65, 92, 67, 69, 69, 69, 69, 73, 73, 73, 73,
   68, 78, 79, 79, 79, 79, 79,215,216, 85, 85, 85, 85, 89,222, 83,
   65, 65, 65, 65, 65, 65, 92, 67, 69, 69, 69, 69, 73, 73, 73, 73,
   68, 78, 79, 79, 79, 79, 79,247,216, 85, 85, 85, 85, 89,222, 89
};

#define L1_AE 196
#define L1_ae 228
#define L1_OE 214
#define L1_oe 246
#define L1_UE 220
#define L1_ue 252
#define L1_ss 223

int my_strnncoll_latin1_de(const uchar * s1, int len1,
                           const uchar * s2, int len2)
{
  const uchar *e1 = s1 + len1;
  const uchar *e2 = s2 + len2;

  while (s1 < e1 && s2 < e2)
  {
    /* to_upper is used instead of sort_order, because we don't want
     * 'Ä' to match "ÁE", only "AE".  This couples the to_upper and
     * sort_order tables together, but that is acceptable. */
    uchar c1 = to_upper_latin1_de[*s1];
    uchar c2 = to_upper_latin1_de[*s2];
    if (c1 != c2)
    {
      switch (c1)
      {

#define CHECK_S1_COMBO(fst, snd, accent)                                  \
  /* Invariant: c1 == fst == sort_order_latin1_de[accent] && c1 != c2 */  \
  if (c2 == accent)                                                       \
  {                                                                       \
    if (s1 + 1 < e1)                                                      \
    {                                                                     \
      if (to_upper_latin1_de[*(s1 + 1)] == snd)                           \
      {                                                                   \
	/* They are equal (e.g., "Ae" == 'ä') */                          \
	s1 += 2;                                                          \
	s2 += 1;                                                          \
      }                                                                   \
      else                                                                \
      {                                                                   \
	int diff = sort_order_latin1_de[*(s1 + 1)] - snd;                 \
	if (diff)                                                         \
	  return diff;                                                    \
	else                                                              \
	  /* Comparison between, e.g., "AÉ" and 'Ä' */                    \
	  return 1;                                                       \
      }                                                                   \
    }                                                                     \
    else                                                                  \
      return -1;                                                          \
  }                                                                       \
  else                                                                    \
    /* The following should work even if c2 is [ÄÖÜß] */                  \
    return fst - sort_order_latin1_de[c2]

      case 'A':
	CHECK_S1_COMBO('A', 'E', L1_AE);
	break;
      case 'O':
	CHECK_S1_COMBO('O', 'E', L1_OE);
	break;
      case 'U':
	CHECK_S1_COMBO('U', 'E', L1_UE);
	break;
      case 'S':
	CHECK_S1_COMBO('S', 'S', L1_ss);
	break;

#define CHECK_S2_COMBO(fst, snd)                                          \
  /* Invariant: sort_order_latin1_de[c1] == fst && c1 != c2 */            \
  if (c2 == fst)                                                          \
  {                                                                       \
    if (s2 + 1 < e2)                                                      \
    {                                                                     \
      if (to_upper_latin1_de[*(s2 + 1)] == snd)                           \
      {                                                                   \
	/* They are equal (e.g., 'ä' == "Ae") */                          \
	s1 += 1;                                                          \
	s2 += 2;                                                          \
      }                                                                   \
      else                                                                \
      {                                                                   \
	int diff = sort_order_latin1_de[*(s1 + 1)] - snd;                 \
	if (diff)                                                         \
	  return diff;                                                    \
	else                                                              \
	  /* Comparison between, e.g., 'Ä' and "AÉ" */                    \
	  return -1;                                                      \
      }                                                                   \
    }                                                                     \
    else                                                                  \
      return 1;                                                           \
  }                                                                       \
  else                                                                    \
    /* The following should work even if c2 is [ÄÖÜß] */                  \
    return fst - sort_order_latin1_de[c2]

      case L1_AE:
	CHECK_S2_COMBO('A', 'E');
	break;
      case L1_OE:
	CHECK_S2_COMBO('O', 'E');
	break;
      case L1_UE:
	CHECK_S2_COMBO('U', 'E');
	break;
      case L1_ss:
	CHECK_S2_COMBO('S', 'S');
	break;
      default:
	switch (c2) {
	case L1_AE:
	case L1_OE:
	case L1_UE:
	case L1_ss:
	  /* Make sure these do not match (e.g., "Ä" != "Á") */
	  return sort_order_latin1_de[c1] - sort_order_latin1_de[c2];
	  break;
	default:
	  if (sort_order_latin1_de[*s1] != sort_order_latin1_de[*s2])
	    return sort_order_latin1_de[*s1] - sort_order_latin1_de[*s2];
	  ++s1;
	  ++s2;
	  break;
	}
	break;

#undef CHECK_S1_COMBO
#undef CHECK_S2_COMBO

      }
    }
    else
    {
      /* In order to consistently treat "ae" == 'ä', but to NOT allow
       * "aé" == 'ä', we must look ahead here to ensure that the second
       * letter in a combo really is the unaccented 'e' (or 's' for
       * "ss") and is not an accented character with the same sort_order. */
      ++s1;
      ++s2;
      if (s1 < e1 && s2 < e2)
      {
	switch (c1)
	{
	case 'A':
	case 'O':
	case 'U':
	  if (sort_order_latin1_de[*s1] == 'E' &&
	      to_upper_latin1_de[*s1] != 'E' &&
	      to_upper_latin1_de[*s2] == 'E')
	    /* Comparison between, e.g., "AÉ" and "AE" */
	    return 1;
	  if (sort_order_latin1_de[*s2] == 'E' &&
	      to_upper_latin1_de[*s2] != 'E' &&
	      to_upper_latin1_de[*s1] == 'E')
	    /* Comparison between, e.g., "AE" and "AÉ" */
	    return -1;
	  break;
	case 'S':
	  if (sort_order_latin1_de[*s1] == 'S' &&
	      to_upper_latin1_de[*s1] != 'S' &&
	      to_upper_latin1_de[*s2] == 'S')
	    /* Comparison between, e.g., "Sß" and "SS" */
	    return 1;
	  if (sort_order_latin1_de[*s2] == 'S' &&
	      to_upper_latin1_de[*s2] != 'S' &&
	      to_upper_latin1_de[*s1] == 'S')
	    /* Comparison between, e.g., "SS" and "Sß" */
	    return -1;
	  break;
	default:
	  break;
	}
      }
    }
  }

  /* A simple test of string lengths won't work -- we test to see
   * which string ran out first */
  return s1 < e1 ? 1 : s2 < e2 ? -1 : 0;
}

int my_strnxfrm_latin1_de(uchar * dest, const uchar * src, int len, int srclen)
{
  const uchar *dest_orig = dest;
  const uchar *de = dest + len;
  const uchar *se = src + srclen;
  while (src < se && dest < de)
  {
    switch (*src)
    {
    case L1_AE:
    case L1_ae:
      *dest++ = 'A';
      if (dest < de)
	*dest++ = 'E';
      break;
    case L1_OE:
    case L1_oe:
      *dest++ = 'O';
      if (dest < de)
	*dest++ = 'E';
      break;
    case L1_UE:
    case L1_ue:
      *dest++ = 'U';
      if (dest < de)
	*dest++ = 'E';
      break;
    case L1_ss:
      *dest++ = 'S';
      if (dest < de)
	*dest++ = 'S';
      break;
    default:
      *dest++ = sort_order_latin1_de[*src];
      break;
    }
    ++src;
  }
  return dest - dest_orig;
}

int my_strcoll_latin1_de(const uchar * s1, const uchar * s2)
{
  /* XXX QQ: This should be fixed to not call strlen */
  return my_strnncoll_latin1_de(s1, strlen(s1), s2, strlen(s2));
}

int my_strxfrm_latin1_de(uchar * dest, const uchar * src, int len)
{
  /* XXX QQ: This should be fixed to not call strlen */
  return my_strnxfrm_latin1_de(dest, src, len, strlen(src));
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
#define max_sort_char ((char) 255)
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

  for (; ptr != end && min_str != min_end; ++ptr)
  {
    if (*ptr == escape && ptr + 1 != end)
    {
      ++ptr;				/* Skip escape */
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
	*min_str++ = min_sort_char;
	*max_str++ = max_sort_char;
      } while (min_str != min_end);
      return 0;
    }
    *min_str++ = *max_str++ = *ptr;
  }
  *min_length = *max_length = (uint) (min_str - min_org);
  while (min_str != min_end)
  {
    *min_str++ = ' ';			/* For proper key compression */
    *max_str++ = ' ';
  }
  return 0;
}
