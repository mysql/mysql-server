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

/* This file is for Shift JIS charset, and created by tommy@valley.ne.jp.
 */

#include <my_global.h>
#include "m_string.h"
#include "m_ctype.h"

/*
 * This comment is parsed by configure to create ctype.c,
 * so don't change it unless you know what you are doing.
 *
 * .configure. strxfrm_multiply_sjis=1
 * .configure. mbmaxlen_sjis=2
 */

uchar NEAR ctype_sjis[257] =
{
    0,				/* For standard library */
    0040, 0040, 0040, 0040, 0040, 0040, 0040, 0040,	/* NUL ^A - ^G */
    0040, 0050, 0050, 0050, 0050, 0050, 0040, 0040,	/* ^H - ^O */
    0040, 0040, 0040, 0040, 0040, 0040, 0040, 0040,	/* ^P - ^W */
    0040, 0040, 0040, 0040, 0040, 0040, 0040, 0040,	/* ^X - ^Z ^[ ^\ ^] ^^ ^_ */
    0110, 0020, 0020, 0020, 0020, 0020, 0020, 0020,	/* SPC ! " # $ % ^ ' */
    0020, 0020, 0020, 0020, 0020, 0020, 0020, 0020,	/* ( ) * + , - . / */
    0204, 0204, 0204, 0204, 0204, 0204, 0204, 0204,	/* 0 1 2 3 4 5 6 7 */
    0204, 0204, 0020, 0020, 0020, 0020, 0020, 0020,	/* 8 9 : ; < = > ? */
    0020, 0201, 0201, 0201, 0201, 0201, 0201, 0001,	/* @ A B C D E F G */
    0001, 0001, 0001, 0001, 0001, 0001, 0001, 0001,	/* H I J K L M N O */
    0001, 0001, 0001, 0001, 0001, 0001, 0001, 0001,	/* P Q R S T U V W */
    0001, 0001, 0001, 0020, 0020, 0020, 0020, 0020,	/* X Y Z [ \ ] ^ _ */
    0020, 0202, 0202, 0202, 0202, 0202, 0202, 0002,	/* ` a b c d e f g */
    0002, 0002, 0002, 0002, 0002, 0002, 0002, 0002,	/* h i j k l m n o */
    0002, 0002, 0002, 0002, 0002, 0002, 0002, 0002,	/* p q r s t u v w */
    0002, 0002, 0002, 0020, 0020, 0020, 0020, 0040,	/* x y z { | } + DEL */
    0020, 0020, 0020, 0020, 0020, 0020, 0020, 0020,
    0020, 0020, 0020, 0020, 0020, 0020, 0020, 0020,
    0020, 0020, 0020, 0020, 0020, 0020, 0020, 0020,
    0020, 0020, 0020, 0020, 0020, 0020, 0020, 0020,
    0020, 0020, 0020, 0020, 0020, 0020, 0020, 0020,
    0020, 0020, 0020, 0020, 0020, 0020, 0020, 0020,
    0020, 0020, 0020, 0020, 0020, 0020, 0020, 0020,
    0020, 0020, 0020, 0020, 0020, 0020, 0020, 0020,
    0020, 0020, 0020, 0020, 0020, 0020, 0020, 0020,
    0020, 0020, 0020, 0020, 0020, 0020, 0020, 0020,
    0020, 0020, 0020, 0020, 0020, 0020, 0020, 0020,
    0020, 0020, 0020, 0020, 0020, 0020, 0020, 0020,
    0020, 0020, 0020, 0020, 0020, 0020, 0020, 0020,
    0020, 0020, 0020, 0020, 0020, 0020, 0020, 0020,
    0020, 0020, 0020, 0020, 0020, 0020, 0020, 0020,
    0020, 0020, 0020, 0020, 0020, 0000, 0000, 0000
};

uchar NEAR to_lower_sjis[]=
{
  '\000','\001','\002','\003','\004','\005','\006','\007',
  '\010','\011','\012','\013','\014','\015','\016','\017',
  '\020','\021','\022','\023','\024','\025','\026','\027',
  '\030','\031','\032','\033','\034','\035','\036','\037',
  ' ',	 '!',	'"',   '#',   '$',   '%',   '&',   '\'',
  '(',	 ')',	'*',   '+',   ',',   '-',   '.',   '/',
  '0',	 '1',	'2',   '3',   '4',   '5',   '6',   '7',
  '8',	 '9',	':',   ';',   '<',   '=',   '>',   '?',
  '@',	 'a',	'b',   'c',   'd',   'e',   'f',   'g',
  'h',	 'i',	'j',   'k',   'l',   'm',   'n',   'o',
  'p',	 'q',	'r',   's',   't',   'u',   'v',   'w',
  'x',	 'y',	'z',   '[',   '\\',  ']',   '^',   '_',
  '`',	 'a',	'b',   'c',   'd',   'e',   'f',   'g',
  'h',	 'i',	'j',   'k',   'l',   'm',   'n',   'o',
  'p',	 'q',	'r',   's',   't',   'u',   'v',   'w',
  'x',	 'y',	'z',   '{',   '|',   '}',   '~',   '\177',
  (uchar) '\200',(uchar) '\201',(uchar) '\202',(uchar) '\203',(uchar) '\204',(uchar) '\205',(uchar) '\206',(uchar) '\207',
  (uchar) '\210',(uchar) '\211',(uchar) '\212',(uchar) '\213',(uchar) '\214',(uchar) '\215',(uchar) '\216',(uchar) '\217',
  (uchar) '\220',(uchar) '\221',(uchar) '\222',(uchar) '\223',(uchar) '\224',(uchar) '\225',(uchar) '\226',(uchar) '\227',
  (uchar) '\230',(uchar) '\231',(uchar) '\232',(uchar) '\233',(uchar) '\234',(uchar) '\235',(uchar) '\236',(uchar) '\237',
  (uchar) '\240',(uchar) '\241',(uchar) '\242',(uchar) '\243',(uchar) '\244',(uchar) '\245',(uchar) '\246',(uchar) '\247',
  (uchar) '\250',(uchar) '\251',(uchar) '\252',(uchar) '\253',(uchar) '\254',(uchar) '\255',(uchar) '\256',(uchar) '\257',
  (uchar) '\260',(uchar) '\261',(uchar) '\262',(uchar) '\263',(uchar) '\264',(uchar) '\265',(uchar) '\266',(uchar) '\267',
  (uchar) '\270',(uchar) '\271',(uchar) '\272',(uchar) '\273',(uchar) '\274',(uchar) '\275',(uchar) '\276',(uchar) '\277',
  (uchar) '\300',(uchar) '\301',(uchar) '\302',(uchar) '\303',(uchar) '\304',(uchar) '\305',(uchar) '\306',(uchar) '\307',
  (uchar) '\310',(uchar) '\311',(uchar) '\312',(uchar) '\313',(uchar) '\314',(uchar) '\315',(uchar) '\316',(uchar) '\317',
  (uchar) '\320',(uchar) '\321',(uchar) '\322',(uchar) '\323',(uchar) '\324',(uchar) '\325',(uchar) '\326',(uchar) '\327',
  (uchar) '\330',(uchar) '\331',(uchar) '\332',(uchar) '\333',(uchar) '\334',(uchar) '\335',(uchar) '\336',(uchar) '\337',
  (uchar) '\340',(uchar) '\341',(uchar) '\342',(uchar) '\343',(uchar) '\344',(uchar) '\345',(uchar) '\346',(uchar) '\347',
  (uchar) '\350',(uchar) '\351',(uchar) '\352',(uchar) '\353',(uchar) '\354',(uchar) '\355',(uchar) '\356',(uchar) '\357',
  (uchar) '\360',(uchar) '\361',(uchar) '\362',(uchar) '\363',(uchar) '\364',(uchar) '\365',(uchar) '\366',(uchar) '\367',
  (uchar) '\370',(uchar) '\371',(uchar) '\372',(uchar) '\373',(uchar) '\374',(uchar) '\375',(uchar) '\376',(uchar) '\377'
};

uchar NEAR to_upper_sjis[]=
{
  '\000','\001','\002','\003','\004','\005','\006','\007',
  '\010','\011','\012','\013','\014','\015','\016','\017',
  '\020','\021','\022','\023','\024','\025','\026','\027',
  '\030','\031','\032','\033','\034','\035','\036','\037',
  ' ',	 '!',	'"',   '#',   '$',   '%',   '&',   '\'',
  '(',	 ')',	'*',   '+',   ',',   '-',   '.',   '/',
  '0',	 '1',	'2',   '3',   '4',   '5',   '6',   '7',
  '8',	 '9',	':',   ';',   '<',   '=',   '>',   '?',
  '@',	 'A',	'B',   'C',   'D',   'E',   'F',   'G',
  'H',	 'I',	'J',   'K',   'L',   'M',   'N',   'O',
  'P',	 'Q',	'R',   'S',   'T',   'U',   'V',   'W',
  'X',	 'Y',	'Z',   '[',   '\\',  ']',   '^',   '_',
  '`',	 'A',	'B',   'C',   'D',   'E',   'F',   'G',
  'H',	 'I',	'J',   'K',   'L',   'M',   'N',   'O',
  'P',	 'Q',	'R',   'S',   'T',   'U',   'V',   'W',
  'X',	 'Y',	'Z',   '{',   '|',   '}',   '~',   '\177',
  (uchar) '\200',(uchar) '\201',(uchar) '\202',(uchar) '\203',(uchar) '\204',(uchar) '\205',(uchar) '\206',(uchar) '\207',
  (uchar) '\210',(uchar) '\211',(uchar) '\212',(uchar) '\213',(uchar) '\214',(uchar) '\215',(uchar) '\216',(uchar) '\217',
  (uchar) '\220',(uchar) '\221',(uchar) '\222',(uchar) '\223',(uchar) '\224',(uchar) '\225',(uchar) '\226',(uchar) '\227',
  (uchar) '\230',(uchar) '\231',(uchar) '\232',(uchar) '\233',(uchar) '\234',(uchar) '\235',(uchar) '\236',(uchar) '\237',
  (uchar) '\240',(uchar) '\241',(uchar) '\242',(uchar) '\243',(uchar) '\244',(uchar) '\245',(uchar) '\246',(uchar) '\247',
  (uchar) '\250',(uchar) '\251',(uchar) '\252',(uchar) '\253',(uchar) '\254',(uchar) '\255',(uchar) '\256',(uchar) '\257',
  (uchar) '\260',(uchar) '\261',(uchar) '\262',(uchar) '\263',(uchar) '\264',(uchar) '\265',(uchar) '\266',(uchar) '\267',
  (uchar) '\270',(uchar) '\271',(uchar) '\272',(uchar) '\273',(uchar) '\274',(uchar) '\275',(uchar) '\276',(uchar) '\277',
  (uchar) '\300',(uchar) '\301',(uchar) '\302',(uchar) '\303',(uchar) '\304',(uchar) '\305',(uchar) '\306',(uchar) '\307',
  (uchar) '\310',(uchar) '\311',(uchar) '\312',(uchar) '\313',(uchar) '\314',(uchar) '\315',(uchar) '\316',(uchar) '\317',
  (uchar) '\320',(uchar) '\321',(uchar) '\322',(uchar) '\323',(uchar) '\324',(uchar) '\325',(uchar) '\326',(uchar) '\327',
  (uchar) '\330',(uchar) '\331',(uchar) '\332',(uchar) '\333',(uchar) '\334',(uchar) '\335',(uchar) '\336',(uchar) '\337',
  (uchar) '\340',(uchar) '\341',(uchar) '\342',(uchar) '\343',(uchar) '\344',(uchar) '\345',(uchar) '\346',(uchar) '\347',
  (uchar) '\350',(uchar) '\351',(uchar) '\352',(uchar) '\353',(uchar) '\354',(uchar) '\355',(uchar) '\356',(uchar) '\357',
  (uchar) '\360',(uchar) '\361',(uchar) '\362',(uchar) '\363',(uchar) '\364',(uchar) '\365',(uchar) '\366',(uchar) '\367',
  (uchar) '\370',(uchar) '\371',(uchar) '\372',(uchar) '\373',(uchar) '\374',(uchar) '\375',(uchar) '\376',(uchar) '\377'
};

uchar NEAR sort_order_sjis[]=
{
  '\000','\001','\002','\003','\004','\005','\006','\007',
  '\010','\011','\012','\013','\014','\015','\016','\017',
  '\020','\021','\022','\023','\024','\025','\026','\027',
  '\030','\031','\032','\033','\034','\035','\036','\037',
  ' ',	 '!',	'"',   '#',   '$',   '%',   '&',   '\'',
  '(',	 ')',	'*',   '+',   ',',   '-',   '.',   '/',
  '0',	 '1',	'2',   '3',   '4',   '5',   '6',   '7',
  '8',	 '9',	':',   ';',   '<',   '=',   '>',   '?',
  '@',	 'A',	'B',   'C',   'D',   'E',   'F',   'G',
  'H',	 'I',	'J',   'K',   'L',   'M',   'N',   'O',
  'P',	 'Q',	'R',   'S',   'T',   'U',   'V',   'W',
  'X',	 'Y',	'Z',   '[',   '\\',  ']',   '^',   '_',
  '`',	 'A',	'B',   'C',   'D',   'E',   'F',   'G',
  'H',	 'I',	'J',   'K',   'L',   'M',   'N',   'O',
  'P',	 'Q',	'R',   'S',   'T',   'U',   'V',   'W',
  'X',	 'Y',	'Z',   '{',   '|',   '}',   '~',   '\177',
  (uchar) '\200',(uchar) '\201',(uchar) '\202',(uchar) '\203',(uchar) '\204',(uchar) '\205',(uchar) '\206',(uchar) '\207',
  (uchar) '\210',(uchar) '\211',(uchar) '\212',(uchar) '\213',(uchar) '\214',(uchar) '\215',(uchar) '\216',(uchar) '\217',
  (uchar) '\220',(uchar) '\221',(uchar) '\222',(uchar) '\223',(uchar) '\224',(uchar) '\225',(uchar) '\226',(uchar) '\227',
  (uchar) '\230',(uchar) '\231',(uchar) '\232',(uchar) '\233',(uchar) '\234',(uchar) '\235',(uchar) '\236',(uchar) '\237',
  (uchar) '\240',(uchar) '\241',(uchar) '\242',(uchar) '\243',(uchar) '\244',(uchar) '\245',(uchar) '\246',(uchar) '\247',
  (uchar) '\250',(uchar) '\251',(uchar) '\252',(uchar) '\253',(uchar) '\254',(uchar) '\255',(uchar) '\256',(uchar) '\257',
  (uchar) '\260',(uchar) '\261',(uchar) '\262',(uchar) '\263',(uchar) '\264',(uchar) '\265',(uchar) '\266',(uchar) '\267',
  (uchar) '\270',(uchar) '\271',(uchar) '\272',(uchar) '\273',(uchar) '\274',(uchar) '\275',(uchar) '\276',(uchar) '\277',
  (uchar) '\300',(uchar) '\301',(uchar) '\302',(uchar) '\303',(uchar) '\304',(uchar) '\305',(uchar) '\306',(uchar) '\307',
  (uchar) '\310',(uchar) '\311',(uchar) '\312',(uchar) '\313',(uchar) '\314',(uchar) '\315',(uchar) '\316',(uchar) '\317',
  (uchar) '\320',(uchar) '\321',(uchar) '\322',(uchar) '\323',(uchar) '\324',(uchar) '\325',(uchar) '\326',(uchar) '\327',
  (uchar) '\330',(uchar) '\331',(uchar) '\332',(uchar) '\333',(uchar) '\334',(uchar) '\335',(uchar) '\336',(uchar) '\337',
  (uchar) '\340',(uchar) '\341',(uchar) '\342',(uchar) '\343',(uchar) '\344',(uchar) '\345',(uchar) '\346',(uchar) '\347',
  (uchar) '\350',(uchar) '\351',(uchar) '\352',(uchar) '\353',(uchar) '\354',(uchar) '\355',(uchar) '\356',(uchar) '\357',
  (uchar) '\360',(uchar) '\361',(uchar) '\362',(uchar) '\363',(uchar) '\364',(uchar) '\365',(uchar) '\366',(uchar) '\367',
  (uchar) '\370',(uchar) '\371',(uchar) '\372',(uchar) '\373',(uchar) '\374',(uchar) '\375',(uchar) '\376',(uchar) '\377'
};

#define issjishead(c) ((0x81<=(c) && (c)<=0x9f) || \
                       ((0xe0<=(c)) && (c)<=0xfc))
#define issjistail(c) ((0x40<=(c) && (c)<=0x7e) || \
                       (0x80<=(c) && (c)<=0xfc))


int ismbchar_sjis(const char* p, const char *e)
{
  return (issjishead((uchar) *p) && (e-p)>1 && issjistail((uchar)p[1]) ? 2: 0);
}

my_bool ismbhead_sjis(uint c)
{
  return issjishead((uchar) c);
}

int mbcharlen_sjis(uint c)
{
  return (issjishead((uchar) c) ? 2: 0);
}


#define sjiscode(c,d)	((((uint) (uchar)(c)) << 8) | (uint) (uchar) (d))

int my_strnncoll_sjis(const uchar *s1, int len1, const uchar *s2, int len2)
{
  const uchar *e1 = s1 + len1;
  const uchar *e2 = s2 + len2;
  while (s1 < e1 && s2 < e2) {
    if (ismbchar_sjis((char*) s1, (char*) e1) &&
	ismbchar_sjis((char*) s2, (char*) e2)) {
      uint c1 = sjiscode(*s1, *(s1+1));
      uint c2 = sjiscode(*s2, *(s2+1));
      if (c1 != c2)
	return c1 - c2;
      s1 += 2;
      s2 += 2;
    } else {
      if (sort_order_sjis[(uchar)*s1] != sort_order_sjis[(uchar)*s2])
	return sort_order_sjis[(uchar)*s1] - sort_order_sjis[(uchar)*s2];
      s1++;
      s2++;
    }
  }
  return len1 - len2;
}

int my_strcoll_sjis(const uchar *s1, const uchar *s2)
{
  return (uint) my_strnncoll_sjis(s1,(uint) strlen((char*) s1),
				  s2,(uint) strlen((char*) s2));
}

int my_strnxfrm_sjis(uchar *dest, const uchar *src, int len, int srclen)
{
  uchar *d_end = dest + len;
  uchar *s_end = (uchar*) src + srclen;
  while (dest < d_end && src < s_end) {
    if (ismbchar_sjis((char*) src, (char*) s_end)) {
      *dest++ = *src++;
      if (dest < d_end && src < s_end)
	*dest++ = *src++;
    } else {
      *dest++ = sort_order_sjis[(uchar)*src++];
    }
  }
  return srclen;
}

int my_strxfrm_sjis(uchar *dest, const uchar *src, int len)
{
  return my_strnxfrm_sjis(dest, src, len, (uint) strlen((char*) src));
}


/*
** Calculate min_str and max_str that ranges a LIKE string.
** Arguments:
** ptr		Pointer to LIKE string.
** ptr_length	Length of LIKE string.
** escape	Escape character in LIKE.  (Normally '\').
**		All escape characters should be removed from min_str and max_str
** res_length	Length of min_str and max_str.
** min_str	Smallest case sensitive string that ranges LIKE.
**		Should be space padded to res_length.
** max_str	Largest case sensitive string that ranges LIKE.
**		Normally padded with the biggest character sort value.
**
** The function should return 0 if ok and 1 if the LIKE string can't be
** optimized !
*/

#define max_sort_char ((char) 255)
#define wild_one '_'
#define wild_many '%'

my_bool my_like_range_sjis(const char *ptr,uint ptr_length,pchar escape,
                      uint res_length, char *min_str,char *max_str,
                      uint *min_length,uint *max_length)
{
  const char *end=ptr+ptr_length;
  char *min_org=min_str;
  char *min_end=min_str+res_length;

  while (ptr < end && min_str < min_end) {
    if (ismbchar_sjis(ptr, end)) {
      *min_str++ = *max_str++ = *ptr++;
      if (min_str < min_end)
	*min_str++ = *max_str++ = *ptr++;
      continue;
    }
    if (*ptr == escape && ptr+1 < end) {
      ptr++;				/* Skip escape */
      if (ismbchar_sjis(ptr, end))
	*min_str++ = *max_str++ = *ptr++;
      if (min_str < min_end)
	*min_str++ = *max_str++ = *ptr++;
      continue;
    }
    if (*ptr == wild_one) {		/* '_' in SQL */
      *min_str++ = '\0';		/* This should be min char */
      *max_str++ = max_sort_char;
      ptr++;
      continue;
    }
    if (*ptr == wild_many) {		/* '%' in SQL */
      *min_length = (uint)(min_str - min_org);
      *max_length = res_length;
      do {
	*min_str++ = ' ';		/* Because if key compression */
	*max_str++ = max_sort_char;
      } while (min_str < min_end);
      return 0;
    }
    *min_str++ = *max_str++ = *ptr++;
  }
  *min_length = *max_length = (uint)(min_str - min_org);
  while (min_str < min_end)
    *min_str++ = *max_str++ = ' ';	/* Because if key compression */
  return 0;
}
