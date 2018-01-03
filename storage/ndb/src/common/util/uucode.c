/*
   Copyright (C) 2003-2006 MySQL AB
    Use is subject to license terms.

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


#include <ndb_global.h>

/* ENC is the basic 1 character encoding function to make a char printing */
/* DEC is single character decode */
#define	ENC(c) ((c) ? ((c) & 077) + ' ': '`')
#define	DEC(c) (((c) - ' ') & 077)

/*
 * copy from in to out, encoding as you go along.
 */
void
uuencode(const char * data, int dataLen, FILE * out)
{
  int ch, n;
  const char *p = data;

  fprintf(out, "begin\n");
  
  while (dataLen > 0){
    n = dataLen > 45 ? 45 : dataLen;
    dataLen -= n;
    ch = ENC(n);
    if (putc(ch, out) == EOF)
      break;
    for (; n > 0; n -= 3, p += 3) {
      char p_0 = * p;
      char p_1 = 0;
      char p_2 = 0;

      if(n >= 2){
	p_1 = p[1];
      }
      if(n >= 3){
	p_2 = p[2];
      }
      
      ch = p_0 >> 2;
      ch = ENC(ch);
      if (putc(ch, out) == EOF)
	break;
      ch = ((p_0 << 4) & 060) | ((p_1 >> 4) & 017);
      ch = ENC(ch);
      if (putc(ch, out) == EOF)
	break;
      ch = ((p_1 << 2) & 074) | ((p_2 >> 6) & 03);
      ch = ENC(ch);
      if (putc(ch, out) == EOF)
	break;
      ch = p_2 & 077;
      ch = ENC(ch);
      if (putc(ch, out) == EOF)
	break;
    }
    if (putc('\n', out) == EOF)
      break;
  }
  ch = ENC('\0');
  putc(ch, out);
  putc('\n', out);
  fprintf(out, "end\n");
}

int
uudecode(FILE * input, char * outBuf, int bufLen){
  int n;
  char ch, *p, returnCode;
  char buf[255];

  returnCode = 0;
  /* search for header line */
  do {
    if (!fgets(buf, sizeof(buf), input)) {
      return 1;
    }
  } while (strncmp(buf, "begin", 5));
  
  /* for each input line */
  for (;;) {
    if (!fgets(p = buf, sizeof(buf), input)) {
      return 1;
    }
    /*
     * `n' is used to avoid writing out all the characters
     * at the end of the file.
     */
    if ((n = DEC(*p)) <= 0)
      break;
    if(n >= bufLen){
      returnCode = 1;
      break;
    }
    for (++p; n > 0; p += 4, n -= 3)
      if (n >= 3) {
	ch = DEC(p[0]) << 2 | DEC(p[1]) >> 4;
	* outBuf = ch; outBuf++; bufLen--;
	ch = DEC(p[1]) << 4 | DEC(p[2]) >> 2;
	* outBuf = ch; outBuf++; bufLen--;
	ch = DEC(p[2]) << 6 | DEC(p[3]);
	* outBuf = ch; outBuf++; bufLen--;
      } else {
	if (n >= 1) {
	  ch = DEC(p[0]) << 2 | DEC(p[1]) >> 4;
	  * outBuf = ch; outBuf++; bufLen--;
	}
	if (n >= 2) {
	  ch = DEC(p[1]) << 4 | DEC(p[2]) >> 2;
	  * outBuf = ch; outBuf++; bufLen--;
	}
	if (n >= 3) {
	  ch = DEC(p[2]) << 6 | DEC(p[3]);
	  * outBuf = ch; outBuf++; bufLen--;
	}
      }
  }
  if (!fgets(buf, sizeof(buf), input) || strcmp(buf, "end\n")) {
    return 1;
  }
  return returnCode;
}

int 
uuencode_mem(char * dst, const char * data, int dataLen)
{
  int sz = 0;

  int ch, n;
  const char *p = data;
  
  while (dataLen > 0){
    n = dataLen > 45 ? 45 : dataLen;
    dataLen -= n;
    ch = ENC(n);
    * dst = ch; dst++; sz++;
    for (; n > 0; n -= 3, p += 3) {
      char p_0 = * p;
      char p_1 = 0;
      char p_2 = 0;

      if(n >= 2){
	p_1 = p[1];
      }
      if(n >= 3){
	p_2 = p[2];
      }
      
      ch = p_0 >> 2;
      ch = ENC(ch);
      * dst = ch; dst++; sz++;

      ch = ((p_0 << 4) & 060) | ((p_1 >> 4) & 017);
      ch = ENC(ch);
      * dst = ch; dst++; sz++;

      ch = ((p_1 << 2) & 074) | ((p_2 >> 6) & 03);
      ch = ENC(ch);
      * dst = ch; dst++; sz++;

      ch = p_2 & 077;
      ch = ENC(ch);
      * dst = ch; dst++; sz++;
    }
    
    * dst = '\n'; dst++; sz++;
  }
  ch = ENC('\0');
  * dst = ch; dst++; sz++;
  
  * dst = '\n'; dst++; sz++;
  * dst = 0;    dst++; sz++;
  
  return sz;
}

int
uudecode_mem(char * outBuf, int bufLen, const char * src){
  int n;
  char ch;
  int sz = 0;
  const char * p = src;

  /*
   * `n' is used to avoid writing out all the characters
   * at the end of the file.
   */
  if ((n = DEC(*p)) <= 0)
    return 0;
  if(n >= bufLen){
    return -1;
  }
  for (++p; n > 0; p += 4, n -= 3){
    if (n >= 3) {
      ch = DEC(p[0]) << 2 | DEC(p[1]) >> 4;
      * outBuf = ch; outBuf++; bufLen--; sz++;
      ch = DEC(p[1]) << 4 | DEC(p[2]) >> 2;
      * outBuf = ch; outBuf++; bufLen--; sz++;
      ch = DEC(p[2]) << 6 | DEC(p[3]);
      * outBuf = ch; outBuf++; bufLen--; sz++;
    } else {
      if (n >= 1) {
	ch = DEC(p[0]) << 2 | DEC(p[1]) >> 4;
	* outBuf = ch; outBuf++; bufLen--; sz++;
      }
      if (n >= 2) {
	ch = DEC(p[1]) << 4 | DEC(p[2]) >> 2;
	* outBuf = ch; outBuf++; bufLen--; sz++;
      }
      if (n >= 3) {
	ch = DEC(p[2]) << 6 | DEC(p[3]);
	* outBuf = ch; outBuf++; bufLen--; sz++;
      }
    }
  }
  return sz;
}



