/* Copyright (C) 2000 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

/*
        File strings/ctype-czech.c for MySQL.

	This file implements the Czech sorting for the MySQL database
	server (www.mysql.com). Due to some complicated rules the
	Czech language has for sorting strings, a more complex
	solution was needed than the one-to-one conversion table. To
	note a few, here is an example of a Czech sorting sequence:

		co < hlaska < hláska < hlava < chlapec < krtek

	It because some of the rules are: double char 'ch' is sorted
	between 'h' and 'i'. Accented character 'á' (a with acute) is
	sorted after 'a' and before 'b', but only if the word is
	otherwise the same. However, because 's' is sorted before 'v'
	in hlava, the accentness of 'á' is overridden. There are many
	more rules.

	This file defines functions my_strxfrm and my_strcoll for
	C-like zero terminated strings and my_strnxfrm and my_strnncoll
	for strings where the length comes as an parameter. Also
	defined here you will find function my_like_range that returns
	index range strings for LIKE expression and the
	MY_STRXFRM_MULTIPLY set to value 4 -- this is the ratio the
	strings grows during my_strxfrm. The algorithm has four
	passes, that's why we need four times more space for expanded
	string.

	This file also contains the ISO-Latin-2 definitions of
	characters.

	Author: (c) 1997--1998 Jan Pazdziora, adelton@fi.muni.cz
	Jan Pazdziora has a shared copyright for this code

	The original of this file can also be found at
	http://www.fi.muni.cz/~adelton/l10n/

	Bug reports and suggestions are always welcome.
*/

/*
 * This comment is parsed by configure to create ctype.c,
 * so don't change it unless you know what you are doing.
 *
 * .configure. strxfrm_multiply_czech=4
 */

#include <my_global.h>
#include "m_string.h"
#include "m_ctype.h"

#ifdef HAVE_CHARSET_latin2

/*
	These are four tables for four passes of the algorithm. Please see
	below for what are the "special values"
*/

static const uchar *CZ_SORT_TABLE[]=
{
  (const uchar*)
  "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x41\x42\x43\x44\x45\x00\x00"
  "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
  "\x47\x58\x5C\x6A\x77\x6B\x69\x5B\x5E\x5F\x66\x6E\x55\x54\x5A\x67"
  "\x78\x79\x7A\x7B\x7C\x7D\x7E\x7F\x80\x81\x57\x56\x71\x72\x73\x59"
  "\x65\x82\x83\xFF\x86\x87\x88\x89\x8A\x8C\x8D\x8E\x8F\x90\x91\x92"
  "\x94\x95\x96\x98\x9A\x9B\x9D\x9E\x9F\xA0\xA1\x60\x68\x61\x4B\x52"
  "\x49\x82\x83\xFF\x86\x87\x88\x89\x8A\x8C\x8D\x8E\x8F\x90\x91\x92"
  "\x94\x95\x96\x98\x9A\x9B\x9D\x9E\x9F\xA0\xA1\x62\x74\x63\x75\x00"
  "\x00\x00\x00\x00\x00\x46\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
  "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
  "\x48\x82\x4C\x8F\x76\x8F\x98\x64\x4E\x99\x98\x9A\xA1\x53\xA2\xA1"
  "\x6D\x82\x51\x8F\x4A\x8F\x98\x6C\x50\x99\x98\x9A\xA1\x4F\xA2\xA1"
  "\x96\x82\x82\x82\x82\x8F\x84\x84\x85\x87\x87\x87\x87\x8C\x8C\x86"
  "\x86\x91\x91\x92\x92\x92\x92\x70\x97\x9B\x9B\x9B\x9B\xA0\x9A\x98"
  "\x96\x82\x82\x82\x82\x8F\x84\x84\x85\x87\x87\x87\x87\x8C\x8C\x86"
  "\x86\x91\x91\x92\x92\x92\x92\x6F\x97\x9B\x9B\x9B\x9B\xA0\x9A\x4D",

  (const uchar*)
  "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x20\x20\x20\x20\x20\x00\x00"
  "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
  "\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20"
  "\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20"
  "\x20\x20\x20\xFF\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20"
  "\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20"
  "\x20\x20\x20\xFF\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20"
  "\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x00"
  "\x00\x00\x00\x00\x00\x20\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
  "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
  "\x20\x2B\x20\x2C\x20\x25\x22\x20\x20\x25\x2A\x25\x22\x20\x25\x29"
  "\x20\x2B\x20\x2C\x20\x25\x22\x20\x20\x25\x2A\x25\x22\x20\x25\x29"
  "\x22\x22\x24\x23\x27\x22\x22\x2A\x25\x22\x2B\x47\x25\x22\x24\x25"
  "\x2C\x22\x25\x22\x24\x28\x27\x20\x25\x26\x22\x28\x27\x22\x2A\x21"
  "\x22\x22\x24\x23\x27\x22\x22\x2A\x25\x22\x2B\x47\x25\x22\x24\x25"
  "\x2C\x22\x25\x22\x24\x28\x27\x20\x25\x26\x22\x28\x27\x22\x2A\x20",


  (const uchar*)
  "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x03\x03\x03\x03\x03\x00\x00"
  "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
  "\x03\x03\x03\x03\x03\x03\x03\x03\x03\x03\x03\x03\x03\x03\x03\x03"
  "\x03\x03\x03\x03\x03\x03\x03\x03\x03\x03\x03\x03\x03\x03\x03\x03"
  "\x03\x05\x05\xFF\x05\x05\x05\x05\x05\x05\x05\x05\x05\x05\x05\x05"
  "\x05\x05\x05\x05\x05\x05\x05\x05\x05\x05\x05\x03\x03\x03\x03\x03"
  "\x03\x03\x03\xFF\x03\x03\x03\x03\x03\x03\x03\x03\x03\x03\x03\x03"
  "\x03\x03\x03\x03\x03\x03\x03\x03\x03\x03\x03\x03\x03\x03\x03\x00"
  "\x00\x00\x00\x00\x00\x03\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
  "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
  "\x1B\x05\x03\x05\x03\x05\x05\x03\x03\x05\x05\x05\x05\x03\x05\x05"
  "\x03\x03\x03\x03\x03\x03\x03\x03\x03\x03\x03\x03\x03\x03\x03\x03"
  "\x05\x05\x05\x05\x05\x05\x05\x05\x05\x05\x05\x05\x05\x05\x05\x05"
  "\x05\x05\x05\x05\x05\x05\x05\x03\x05\x05\x05\x05\x05\x05\x05\x03"
  "\x03\x03\x03\x03\x03\x03\x03\x03\x03\x03\x03\x03\x03\x03\x03\x03"
  "\x03\x03\x03\x03\x03\x03\x03\x03\x03\x03\x03\x03\x03\x03\x03\x03",

  (const uchar*)
  "\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0A\x0B\x0C\x0D\x0E\x0F"
  "\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1A\x1B\x1C\x1D\x1E\x1F"
  "\x20\x21\x22\x23\x24\x25\x26\x27\x28\x29\x2A\x2B\x2C\x2D\x2E\x2F"
  "\x30\x31\x32\x33\x34\x35\x36\x37\x38\x39\x3A\x3B\x3C\x3D\x3E\x3F"
  "\x40\x41\x42\xFF\x44\x45\x46\x47\x48\x49\x4A\x4B\x4C\x4D\x4E\x4F"
  "\x50\x51\x52\x53\x54\x55\x56\x57\x58\x59\x5A\x5B\x5C\x5D\x5E\x5F"
  "\x60\x61\x62\xFF\x64\x65\x66\x67\x68\x69\x6A\x6B\x6C\x6D\x6E\x6F"
  "\x70\x71\x72\x73\x74\x75\x76\x77\x78\x79\x7A\x7B\x7C\x7D\x7E\x7F"
  "\x80\x81\x82\x83\x84\x85\x86\x87\x88\x89\x8A\x8B\x8C\x8D\x8E\x8F"
  "\x90\x91\x92\x93\x94\x95\x96\x97\x98\x99\x9A\x9B\x9C\x9D\x9E\x9F"
  "\xA0\xA1\xA2\xA3\xA4\xA5\xA6\xA7\xA8\xA9\xAA\xAB\xAC\xAD\xAE\xAF"
  "\xB0\xB1\xB2\xB3\xB4\xB5\xB6\xB7\xB8\xB9\xBA\xBB\xBC\xBD\xBE\xBF"
  "\xC0\xC1\xC2\xC3\xC4\xC5\xC6\xC7\xC8\xC9\xCA\xCB\xCC\xCD\xCE\xCF"
  "\xD0\xD1\xD2\xD3\xD4\xD5\xD6\xD7\xD8\xD9\xDA\xDB\xDC\xDD\xDE\xDF"
  "\xE0\xE1\xE2\xE3\xE4\xE5\xE6\xE7\xE8\xE9\xEA\xEB\xEC\xED\xEE\xEF"
  "\xF0\xF1\xF2\xF3\xF4\xF5\xF6\xF7\xF8\xF9\xFA\xFB\xFC\xFD\xFE\xFF"
};

/*
  These define the values for the double chars that need to be
  sorted as they were single characters -- in Czech these are
  'ch', 'Ch' and 'CH'.
*/

struct wordvalue
{
  const uchar *word;
  const uchar *outvalue;
};

static struct wordvalue doubles[]=
{
  { "ch", (const uchar*) "\x8B\x20\x03\x63" },
  { "Ch", (const uchar*) "\x8B\x20\x04\x43" },
  { "CH", (const uchar*) "\x8B\x20\x05\x43" },
  { "c",  (const uchar*) "\x84\x20\x03\x63" },
  { "C",  (const uchar*) "\x84\x20\x05\x43" },
};


/*
  Define "auto" space character,
  which is used while processing "PAD SPACE" rule,
  when one string is shorter than another string.
  "Auto" space character is lower than a real space
  character on the third level.
*/
static const uchar *virtual_space= "\x47\x20\x02\x20";

/*
        Original comments from the contributor:
        
	Informal description of the algorithm:

	We walk the string left to right.

	The end of the string is either passed as parameter, or is
	*p == 0. This is hidden in the IS_END macro.

	In the first two passes, we compare word by word. So we make
	first and second pass on the first word, first and second pass
	on the second word, etc. If we come to the end of the string
	during the first pass, we need to jump to the last word of the
	second pass.

	End of pass is marked with value 1 on the output.

	For each character, we read it's value from the table.

	If the value is ignore (0), we go straight to the next character.

	If the value is space/end of word (2) and we are in the first
	or second pass, we skip all characters having value 0 -- 2 and
	switch the pass.

	If it's the compose character (255), we check if the double
	exists behind it, find its value.

	We append 0 to the end.

	Neformální popis algoritmu:

	procházíme øetìzec zleva doprava
	konec øetìzce poznáme podle *p == 0
	pokud jsme do¹li na konec øetìzce pøi prùchodu 0, nejdeme na
		zaèátek, ale na ulo¾enou pozici, proto¾e první a druhý
		prùchod bì¾í souèasnì
	konec vstupu (prùchodu) oznaèíme na výstupu hodnotou 1

	naèteme hodnotu z tøídící tabulky
	jde-li o hodnotu ignorovat (0), skoèíme na dal¹í prùchod
	jde-li o hodnotu konec slova (2) a je to prùchod 0 nebo 1,
		pøeskoèíme v¹echny dal¹í 0 -- 2 a prohodíme
		prùchody
	jde-li o kompozitní znak (255), otestujeme, zda následuje
		správný do dvojice, dohledáme správnou hodnotu

	na konci pøipojíme znak 0
*/

/*
  In March 2007 latin2_czech_cs was reworked by Alexander Barkov,
  to suite other MySQL collations better, and to be Falcon compatible.
  
  Changes:
  - Discarded word-by-word comparison on the primary and the secondary level.
    Comparison is now strictly done level-by-level
    (like the Unicode Collation Algorithm (UCA) does).
    
  - Character weights were derived from Unicode 5.0.0 standard.
    This is to make order of punctuation characters and digits
    more consistent with all other MySQL collations and UCA.
    
    The order is now:
    
      Controls, spaces, punctuations, digits, letters.
    
    It previously used to be:
    
      Punctuations, controls, some more punctuations, letters, digits.
    
    NOTE:
    
    A minor difference between this implementations and the UCA:
    
    German "LATIN SMALL LETTER SHARP S" does not expand to "ss".
    It is instead considered as secondary greater than "LATIN LETTER S",
    and thus sorted between "LATIN LETTER S" and "LATIN LETTER S WITH ACUTE".
    This allows to reduce *twice* disk space required for un-indexed
    ORDER BY (using the filesort method).
    
    As neither the original version of latin2_czech_cs 
    expanded "SHARP S" to "ss", nor "SHARP S" is a part of Czech alphabet,
    this behavior should be ok.
    
  - Collation is now "PAD SPACE" like all other MySQL collations.
    It ignores trailing spaces on primary and secondary level.
    
  - SPACE and TAB characters are not ignorable anymore.
    Also, they have different weights on primary level,
    like in all other MySQL collations:
    
    SELECT 'a\t' < 'a ' -- returns true
    SELECT 'a\t' < 'a'  -- returns true
    
  - Some other punctuation characters are not ignorable anymore,
    for better compatibility with UCA and other MySQL collations.

*/


#define ADD_TO_RESULT(dest, len, totlen, value)			\
if ((totlen) < (len)) { dest[totlen] = value; } (totlen++);
#define IS_END(p, src, len)	(((char *)p - (char *)src) >= (len))

/*
  src  - IN     pointer to the beginning of the string
  p    - IN/OUT pointer to the current character being processed
  pass - IN     pass number [0..3]
                0 - primary level
                1 - secondary level
                2 - tertiary level 
                3 - quarternary level
  value - OUT   the next weight value.
                -1 is returned on end-of-line.
                 1 is returned between levels ("level separator").
                Any value greater than 1 is a normal weight.
  ml    - IN    a flag indicating whether to switch automatically
                to the secondary level and higher levels,
                or stop at the primary level.
                ml=0 is used for prefix comparison.
*/
                
#define NEXT_CMP_VALUE(src, p, pass, value, len, ml)	\
while (1)						\
{							\
  if (IS_END(p, src, len))				\
  {							\
    /* when we are at the end of string */		\
    /* return either -1 for end of string */		\
   /* or 1 for end of pass */				\
                                                        \
   /* latin2_czech_cs WEIGHT_STRING() returns level */  \
   /* separators even for empty string: 01.01.01.00 */  \
   /* The another Czech collation (cp1250_czech_cs) */  \
   /* returns *empty* WEIGHT_STRING() for empty input.*/\
   /* This is why the below if(){}else{} code block */  \
   /* differs from the similar piece in             */  \
   /* ctype-win1250.c                               */  \
   if (pass != 3 && ml)					\
   {							\
     p= src;						\
     pass++;						\
     value= 1; /* Level separator */                    \
   }                                                    \
   else                                                 \
   {                                                    \
     value= -1; /* End-of-line marker*/                 \
   }							\
   break;						\
  }							\
  /* not at end of string */				\
  value = CZ_SORT_TABLE[pass][*p];			\
  if (value == 0 && pass < 3)				\
  { p++; continue; } /* ignore value on levels 0,1,2 */	\
  if (value == 255)					\
  {							\
    int i;						\
    for (i= 0; i < (int) array_elements(doubles); i++)  \
    {							\
      const char * pattern = doubles[i].word;		\
      const char * q = (const char *) p;		\
      int j = 0;					\
      while (pattern[j])				\
      {							\
	if (IS_END(q, src, len) || (*q != pattern[j]))	\
	 break;						\
	j++; q++;					\
      }							\
      if (!(pattern[j]))				\
      {							\
	value = (int)(doubles[i].outvalue[pass]);	\
	p= (const uchar *) q - 1;			\
	break;						\
      }							\
    }							\
  }							\
  p++;							\
  break;						\
}

/*
  Function strnncoll, actually strcoll, with Czech sorting, which expect
  the length of the strings being specified
*/

static int my_strnncoll_czech(CHARSET_INFO *cs __attribute__((unused)),
			      const uchar *s1, size_t len1, 
			      const uchar *s2, size_t len2,
                              my_bool s2_is_prefix)
{
  int v1, v2;
  const uchar * p1, * p2;
  int pass1= 0, pass2= 0;

  if (s2_is_prefix && len1 > len2)
    len1=len2;

  p1= s1;
  p2= s2;

  do
  {
    int diff;
    NEXT_CMP_VALUE(s1, p1, pass1, v1, (int)len1, 1);
    NEXT_CMP_VALUE(s2, p2, pass2, v2, (int)len2, 1);
    if ((diff = v1 - v2))
      return diff;
  }
  while (v1 >= 0);
  return 0;
}



/*
  Compare strings, ignore trailing spaces
*/

static int
my_strnncollsp_czech(CHARSET_INFO * cs __attribute__((unused)),
                     const uchar *s, size_t slen,
                     const uchar *t, size_t tlen,
                     my_bool diff_if_only_endspace_difference
                     __attribute__((unused)))
{
  int level;

  for (level= 0; level <= 3; level++)
  {
    const uchar *s1= s;
    const uchar *t1= t;

    for (;;)
    {
      int sval, tval, diff;
      NEXT_CMP_VALUE(s, s1, level, sval, (int) slen, 0);
      NEXT_CMP_VALUE(t, t1, level, tval, (int) tlen, 0);
      if (sval < 0)
      {
        sval= virtual_space[level];
        for (; tval >= 0 ;)
        {
          if ((diff= sval - tval))
            return diff;
          NEXT_CMP_VALUE(t, t1, level, tval, (int) tlen, 0);
        }
        break;
      }
      else if (tval < 0)
      {
        tval= virtual_space[level];
        for (; sval >= 0 ;)
        {
          if ((diff= sval - tval))
            return diff;
          NEXT_CMP_VALUE(s, s1, level, sval, (int) slen, 0);
        }
        break;
      }

      if ((diff= sval - tval))
        return diff;
    }
  }
  return 0;
}


/*
  Returns the number of bytes required for strnxfrm().
*/
static size_t
my_strnxfrmlen_czech(CHARSET_INFO *cs __attribute__((unused)), size_t len)
{
  return len * 4 + 4;
}


/*
  Function strnxfrm, actually strxfrm, with Czech sorting, which expect
  the length of the strings being specified
*/
static size_t
my_strnxfrm_czech(CHARSET_INFO * cs  __attribute__((unused)),
                  uchar *dst, size_t dstlen, uint nweights_arg,
                  const uchar *src, size_t srclen, uint flags)
{
  uint level;
  uchar *dst0= dst;
  uchar *de= dst + dstlen;

  if (!(flags & 0x0F)) /* All levels by default */
    flags|= 0x0F;

  for (level= 0; level <= 3; level++)
  {
    if (flags & (1 << level))
    {
      uint nweights= nweights_arg;
      const uchar *p= src;
      int value;
      uchar *dstl= dst;
      
      for (; dst < de && nweights; nweights--)
      {
        NEXT_CMP_VALUE(src, p, level, value, (int) srclen, 0);
        if (value < 0)
          break;
        *dst++= value;
      }
      
      if (dst < de && nweights && (flags & MY_STRXFRM_PAD_WITH_SPACE))
      {
        uint pad_length= de - dst;
        set_if_smaller(pad_length, nweights);
        /* fill with weight for space character */
        bfill(dst, pad_length, virtual_space[level]);
        dst+= pad_length;
      }
      
      my_strxfrm_desc_and_reverse(dstl, dst, flags, level);
      
      /* Add level delimiter */
      if (dst < de)
        *dst++= level < 3 ? 1 : 0;
    }
  }
  if ((flags & MY_STRXFRM_PAD_TO_MAXLEN) && dst < de)
  {
    uint fill_length= de - dst;
    cs->cset->fill(cs, (char*) dst, fill_length, 0);
    dst= de;
  }
  return dst - dst0;
}


#undef IS_END


/*
 */


/*
** Calculate min_str and max_str that ranges a LIKE string.
** Arguments:
** ptr		Pointer to LIKE string.
** ptr_length	Length of LIKE string.
** escape	Escape character in LIKE.  (Normally '\').
**		All escape characters should be removed from min_str and max_str
** res_length   Length of min_str and max_str.
** min_str      Smallest case sensitive string that ranges LIKE.
**		Should be space padded to res_length.
** max_str	Largest case sensitive string that ranges LIKE.
**		Normally padded with the biggest character sort value.
**
** The function should return 0 if ok and 1 if the LIKE string can't be
** optimized !
*/

#define min_sort_char 0x00
#define max_sort_char 0xAE


static my_bool my_like_range_czech(CHARSET_INFO *cs __attribute__((unused)),
				   const char *ptr,size_t ptr_length,
				   pbool escape, pbool w_one, pbool w_many,
				   size_t res_length, char *min_str,
				   char *max_str,
				   size_t *min_length,size_t *max_length)
{
  uchar value;
  const char *end=ptr+ptr_length;
  char *min_org=min_str;
  char *min_end=min_str+res_length;

  for (; ptr != end && min_str != min_end ; ptr++)
  {
    if (*ptr == w_one)		/* '_' in SQL */
    { break; }
    if (*ptr == w_many)		/* '%' in SQL */
    { break; }

    if (*ptr == escape && ptr+1 != end)
    { ptr++; }			/* Skip escape */

    value = CZ_SORT_TABLE[0][(int) (uchar) *ptr];

    if (value == 0)			/* Ignore in the first pass */
    { continue; }
    if (value <= 2)			/* End of pass or end of string */
    { break; }
    if (value == 255)		/* Double char too compicated */
    { break; }

    *min_str++= *max_str++ = *ptr;
  }

  if (cs->state & MY_CS_BINSORT)
    *min_length= (size_t) (min_str - min_org);
  else
  {
    /* 'a\0\0... is the smallest possible string */
    *min_length= res_length;
  }
  /* a\ff\ff... is the biggest possible string */
  *max_length= res_length;

  while (min_str != min_end)
  {
    *min_str++ = min_sort_char;	/* Because of key compression */
    *max_str++ = max_sort_char;
  }
  return 0;
}


/*
 * File generated by cset
 * (C) Abandoned 1997 Zarko Mocnik <zarko.mocnik@dem.si>
 *
 * definition table reworked by Jaromir Dolecek <dolecek@ics.muni.cz>
 */

static uchar NEAR ctype_czech[257] = {
0,
 32, 32, 32, 32, 32, 32, 32, 32, 32, 40, 40, 40, 40, 40, 32, 32,
 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32,
 72, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
132,132,132,132,132,132,132,132,132,132, 16, 16, 16, 16, 16, 16,
 16,129,129,129,129,129,129,  1,  1,  1,  1,  1,  1,  1,  1,  1,
  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1, 16, 16, 16, 16, 16,
 16,130,130,130,130,130,130,  2,  2,  2,  2,  2,  2,  2,  2,  2,
  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2, 16, 16, 16, 16, 32,
 32, 32, 32, 32, 32, 32, 32, 32, 40, 40, 40, 40, 40, 32, 32, 32,
 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 72,
  1, 16,  1, 16,  1,  1, 16,  0,  0,  1,  1,  1,  1, 16,  1,  1,
 16,  2, 16,  2, 16,  2,  2, 16, 16,  2,  2,  2,  2, 16,  2,  2,
  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
 16,  1,  1,  1,  1,  1,  1, 16,  1,  1,  1,  1,  1,  1,  1, 16,
  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
  2,  2,  2,  2,  2,  2,  2, 16,  2,  2,  2,  2,  2,  2,  2, 16,
};

static uchar NEAR to_lower_czech[] = {
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
177,161,179,163,181,182,166,167,168,185,186,187,188,173,190,191,
176,177,178,179,180,181,182,183,184,185,186,187,188,189,190,191,
224,225,226,227,228,229,230,231,232,233,234,235,236,237,238,239,
208,241,242,243,244,245,246,215,248,249,250,251,252,253,254,223,
224,225,226,227,228,229,230,231,232,233,234,235,236,237,238,239,
240,241,242,243,244,245,246,247,248,249,250,251,252,253,254,255,
};

static uchar NEAR to_upper_czech[] = {
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
176,160,178,162,180,164,165,183,184,169,170,171,172,189,174,175,
192,193,194,195,196,197,198,199,200,201,202,203,204,205,206,207,
208,209,210,211,212,213,214,215,216,217,218,219,220,221,222,223,
192,193,194,195,196,197,198,199,200,201,202,203,204,205,206,207,
240,209,210,211,212,213,214,247,216,217,218,219,220,221,222,255,
};

static uchar NEAR sort_order_czech[] = {
  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15,
 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47,
 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63,
 64, 65, 71, 72, 76, 78, 83, 84, 85, 86, 90, 91, 92, 96, 97,100,
105,106,107,110,114,117,122,123,124,125,127,131,132,133,134,135,
136, 65, 71, 72, 76, 78, 83, 84, 85, 86, 90, 91, 92, 96, 97,100,
105,106,107,110,114,117,122,123,124,125,127,137,138,139,140,  0,
  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15, 16,
 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,255,
 66,255, 93,255, 94,111,255,255,255,112,113,115,128,255,129,130,
255, 66,255, 93,255, 94,111,255,255,112,113,115,128,255,129,130,
108, 67, 68, 69, 70, 95, 73, 75, 74, 79, 81, 82, 80, 89, 87, 77,
255, 98, 99,101,102,103,104,255,109,119,118,120,121,126,116,255,
108, 67, 68, 69, 70, 95, 73, 75, 74, 79, 81, 82, 80, 89, 88, 77,
255, 98, 99,101,102,103,104,255,109,119,118,120,121,126,116,255,
};

static uint16 tab_8859_2_uni[256]={
     0,0x0001,0x0002,0x0003,0x0004,0x0005,0x0006,0x0007,
0x0008,0x0009,0x000A,0x000B,0x000C,0x000D,0x000E,0x000F,
0x0010,0x0011,0x0012,0x0013,0x0014,0x0015,0x0016,0x0017,
0x0018,0x0019,0x001A,0x001B,0x001C,0x001D,0x001E,0x001F,
0x0020,0x0021,0x0022,0x0023,0x0024,0x0025,0x0026,0x0027,
0x0028,0x0029,0x002A,0x002B,0x002C,0x002D,0x002E,0x002F,
0x0030,0x0031,0x0032,0x0033,0x0034,0x0035,0x0036,0x0037,
0x0038,0x0039,0x003A,0x003B,0x003C,0x003D,0x003E,0x003F,
0x0040,0x0041,0x0042,0x0043,0x0044,0x0045,0x0046,0x0047,
0x0048,0x0049,0x004A,0x004B,0x004C,0x004D,0x004E,0x004F,
0x0050,0x0051,0x0052,0x0053,0x0054,0x0055,0x0056,0x0057,
0x0058,0x0059,0x005A,0x005B,0x005C,0x005D,0x005E,0x005F,
0x0060,0x0061,0x0062,0x0063,0x0064,0x0065,0x0066,0x0067,
0x0068,0x0069,0x006A,0x006B,0x006C,0x006D,0x006E,0x006F,
0x0070,0x0071,0x0072,0x0073,0x0074,0x0075,0x0076,0x0077,
0x0078,0x0079,0x007A,0x007B,0x007C,0x007D,0x007E,     0,
     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,
0x00A0,0x0104,0x02D8,0x0141,0x00A4,0x013D,0x015A,0x00A7,
0x00A8,0x0160,0x015E,0x0164,0x0179,0x00AD,0x017D,0x017B,
0x00B0,0x0105,0x02DB,0x0142,0x00B4,0x013E,0x015B,0x02C7,
0x00B8,0x0161,0x015F,0x0165,0x017A,0x02DD,0x017E,0x017C,
0x0154,0x00C1,0x00C2,0x0102,0x00C4,0x0139,0x0106,0x00C7,
0x010C,0x00C9,0x0118,0x00CB,0x011A,0x00CD,0x00CE,0x010E,
0x0110,0x0143,0x0147,0x00D3,0x00D4,0x0150,0x00D6,0x00D7,
0x0158,0x016E,0x00DA,0x0170,0x00DC,0x00DD,0x0162,0x00DF,
0x0155,0x00E1,0x00E2,0x0103,0x00E4,0x013A,0x0107,0x00E7,
0x010D,0x00E9,0x0119,0x00EB,0x011B,0x00ED,0x00EE,0x010F,
0x0111,0x0144,0x0148,0x00F3,0x00F4,0x0151,0x00F6,0x00F7,
0x0159,0x016F,0x00FA,0x0171,0x00FC,0x00FD,0x0163,0x02D9
};


/* 0000-00FD , 254 chars */
static uchar tab_uni_8859_2_plane00[]={
0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0A,0x0B,0x0C,0x0D,0x0E,0x0F,
0x10,0x11,0x12,0x13,0x14,0x15,0x16,0x17,0x18,0x19,0x1A,0x1B,0x1C,0x1D,0x1E,0x1F,
0x20,0x21,0x22,0x23,0x24,0x25,0x26,0x27,0x28,0x29,0x2A,0x2B,0x2C,0x2D,0x2E,0x2F,
0x30,0x31,0x32,0x33,0x34,0x35,0x36,0x37,0x38,0x39,0x3A,0x3B,0x3C,0x3D,0x3E,0x3F,
0x40,0x41,0x42,0x43,0x44,0x45,0x46,0x47,0x48,0x49,0x4A,0x4B,0x4C,0x4D,0x4E,0x4F,
0x50,0x51,0x52,0x53,0x54,0x55,0x56,0x57,0x58,0x59,0x5A,0x5B,0x5C,0x5D,0x5E,0x5F,
0x60,0x61,0x62,0x63,0x64,0x65,0x66,0x67,0x68,0x69,0x6A,0x6B,0x6C,0x6D,0x6E,0x6F,
0x70,0x71,0x72,0x73,0x74,0x75,0x76,0x77,0x78,0x79,0x7A,0x7B,0x7C,0x7D,0x7E,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0xA0,0x00,0x00,0x00,0xA4,0x00,0x00,0xA7,0xA8,0x00,0x00,0x00,0x00,0xAD,0x00,0x00,
0xB0,0x00,0x00,0x00,0xB4,0x00,0x00,0x00,0xB8,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0xC1,0xC2,0x00,0xC4,0x00,0x00,0xC7,0x00,0xC9,0x00,0xCB,0x00,0xCD,0xCE,0x00,
0x00,0x00,0x00,0xD3,0xD4,0x00,0xD6,0xD7,0x00,0x00,0xDA,0x00,0xDC,0xDD,0x00,0xDF,
0x00,0xE1,0xE2,0x00,0xE4,0x00,0x00,0xE7,0x00,0xE9,0x00,0xEB,0x00,0xED,0xEE,0x00,
0x00,0x00,0x00,0xF3,0xF4,0x00,0xF6,0xF7,0x00,0x00,0xFA,0x00,0xFC,0xFD};

/* 0102-017E , 125 chars */
static uchar tab_uni_8859_2_plane01[]={
0xC3,0xE3,0xA1,0xB1,0xC6,0xE6,0x00,0x00,0x00,0x00,0xC8,0xE8,0xCF,0xEF,0xD0,0xF0,
0x00,0x00,0x00,0x00,0x00,0x00,0xCA,0xEA,0xCC,0xEC,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xC5,0xE5,0x00,0x00,0xA5,0xB5,0x00,0x00,0xA3,
0xB3,0xD1,0xF1,0x00,0x00,0xD2,0xF2,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xD5,0xF5,
0x00,0x00,0xC0,0xE0,0x00,0x00,0xD8,0xF8,0xA6,0xB6,0x00,0x00,0xAA,0xBA,0xA9,0xB9,
0xDE,0xFE,0xAB,0xBB,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xD9,0xF9,0xDB,0xFB,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xAC,0xBC,0xAF,0xBF,0xAE,0xBE};

/* 02C7-02DD ,  23 chars */
static uchar tab_uni_8859_2_plane02[]={
0xB7,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0xA2,0xFF,0x00,0xB2,0x00,0xBD};

static MY_UNI_IDX idx_uni_8859_2[]={
  {0x0000,0x00FD,tab_uni_8859_2_plane00},
  {0x0102,0x017E,tab_uni_8859_2_plane01},
  {0x02C7,0x02DD,tab_uni_8859_2_plane02},
  {0,0,NULL}
};


static MY_COLLATION_HANDLER my_collation_latin2_czech_ci_handler =
{
  NULL,			/* init */
  my_strnncoll_czech,
  my_strnncollsp_czech,
  my_strnxfrm_czech,
  my_strnxfrmlen_czech,
  my_like_range_czech,
  my_wildcmp_bin,
  my_strcasecmp_8bit,
  my_instr_simple,
  my_hash_sort_simple,
  my_propagate_simple
};

CHARSET_INFO my_charset_latin2_czech_ci =
{
    2,0,0,                                      /* number    */
    MY_CS_COMPILED|MY_CS_STRNXFRM|MY_CS_CSSORT, /* state     */
    "latin2",                                   /* cs name   */
    "latin2_czech_cs",                          /* name      */
    "",                                         /* comment   */
    NULL,                                       /* tailoring */
    ctype_czech,
    to_lower_czech,
    to_upper_czech,
    sort_order_czech,
    NULL,		/* contractions */
    NULL,		/* sort_order_big*/
    tab_8859_2_uni,	/* tab_to_uni   */
    idx_uni_8859_2,	/* tab_from_uni */
    my_unicase_default, /* caseinfo     */
    NULL,		/* state_map    */
    NULL,		/* ident_map    */
    4,			/* strxfrm_multiply */
    1,                  /* caseup_multiply  */
    1,                  /* casedn_multiply  */
    1,			/* mbminlen   */
    1,			/* mbmaxlen  */
    0,			/* min_sort_char */
    0,			/* max_sort_char */
    ' ',                /* pad char      */
    0,                  /* escape_with_backslash_is_dangerous */
    4,                  /* levels_for_compare */
    4,                  /* levels_for_order   */
    &my_charset_8bit_handler,
    &my_collation_latin2_czech_ci_handler
};

#endif
