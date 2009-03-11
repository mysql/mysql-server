/*
Licensed Materials - Property of IBM
DB2 Storage Engine Enablement
Copyright IBM Corporation 2007,2008
All rights reserved

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met: 
 (a) Redistributions of source code must retain this list of conditions, the
     copyright notice in section {d} below, and the disclaimer following this
     list of conditions. 
 (b) Redistributions in binary form must reproduce this list of conditions, the
     copyright notice in section (d) below, and the disclaimer following this
     list of conditions, in the documentation and/or other materials provided
     with the distribution. 
 (c) The name of IBM may not be used to endorse or promote products derived from
     this software without specific prior written permission. 
 (d) The text of the required copyright notice is: 
       Licensed Materials - Property of IBM
       DB2 Storage Engine Enablement 
       Copyright IBM Corporation 2007,2008 
       All rights reserved

THIS SOFTWARE IS PROVIDED BY IBM CORPORATION "AS IS" AND ANY EXPRESS OR IMPLIED
WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
SHALL IBM CORPORATION BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
OF SUCH DAMAGE.
*/

/**
  @file
  
  @brief  A direct map optimization of iconv and related functions
          This was show to significantly reduce character conversion cost
          for short strings when compared to calling iconv system code.
*/

#include <stdlib.h>
#include <stdio.h>
#include <wchar.h>
#include <errno.h>
#include <iconv.h>
#include <ctype.h>
#include <stdarg.h>
#include <string.h>
#include <as400_protos.h>

#include "db2i_myconv.h"
#include "db2i_global.h"

int32_t myconvDebug=0;

static	char 	szGetTimeString[20];
static	char *	GetTimeString(time_t	now)
{
  struct tm *	tm;

  now = time(&now);
  tm = (struct tm *) localtime(&now);
  sprintf(szGetTimeString, "%04d/%02d/%02d %02d:%02d:%02d",
          tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
          tm->tm_hour,        tm->tm_min,   tm->tm_sec);

  return szGetTimeString;
}

static MEM_ROOT dmapMemRoot;

void initMyconv()
{
  init_alloc_root(&dmapMemRoot, 0x200, 0);
}

void cleanupMyconv()
{
  free_root(&dmapMemRoot,0);
}


#ifdef DEBUG
/* type:	*/
#define STDOUT_WITH_TIME        -1      /*  to stdout with time	*/
#define STDERR_WITH_TIME	-2      /*  to stderr with time	*/
#define STDOUT_WO_TIME  	1       /* : to stdout		*/
#define STDERR_WO_TIME          2       /* : to stderr		*/


static void MyPrintf(long	type,
		     char *	fmt, ...)
{
  char 		StdoutFN[256];
  va_list	ap;
  char *	p;
  time_t	now;
  FILE *	fd=stderr;

  if (type < 0)
  {
    now = time(&now);
    fprintf(fd, "%s ", GetTimeString(now));
  }
  va_start(ap, fmt);
  vfprintf(fd, fmt, ap);
  va_end(ap);
}
#endif




#define MAX_CONVERTER           128

mycstoccsid(const char* pname)
{
  if (strcmp(pname, "UTF-16")==0)
    return 1200;
  else if (strcmp(pname, "big5")==0)
    return 950;
  else
    return cstoccsid(pname);
}
#define cstoccsid mycstoccsid

static  struct __myconv_rec     myconv_rec      [MAX_CONVERTER];
static  struct __dmap_rec       dmap_rec        [MAX_CONVERTER];

static int      dmap_open(const char *  to,
                          const char *  from,
                          const int32_t idx)
{
  if (myconvIsSBCS(from) && myconvIsSBCS(to)) {
    dmap_rec[idx].codingSchema = DMAP_S2S;
    if ((dmap_rec[idx].dmapS2S = (uchar *) alloc_root(&dmapMemRoot, 0x100)) == NULL) {
#ifdef DEBUG
        MyPrintf(STDERR_WITH_TIME,
                 "dmap_open(%s,%s,%d), CS=%d failed with malloc(), errno = %d in %s at %d\n",
                   to, from, idx, DMAP_S2S, errno, __FILE__,__LINE__);
#endif
      return -1;
    }
    memset(dmap_rec[idx].dmapS2S, 0x00, 0x100);
    myconv_rec[idx].allocatedSize=0x100;

    {
      char      dmapSrc[0x100];
      iconv_t   cd;
      int32_t   i;
      size_t    inBytesLeft=0x100;
      size_t    outBytesLeft=0x100;
      size_t    len;
      char *    inBuf=dmapSrc;
      char *    outBuf=(char *) dmap_rec[idx].dmapS2S;

      if ((cd = iconv_open(to, from)) == (iconv_t) -1) {
#ifdef DEBUG
        MyPrintf(STDERR_WITH_TIME,
                 "dmap_open(%s,%s,%d) failed with iconv_open(), errno = %d in %s at %d\n",
                   to, from, idx, errno, __FILE__,__LINE__);
#endif
        return -1;
      }

      inBytesLeft = 0x100;
      for (i = 0; i < inBytesLeft; ++i)
        dmapSrc[i]=i;

      do {
        if ((len = iconv(cd, &inBuf, &inBytesLeft, &outBuf, &outBytesLeft)) != (size_t) 0) {
#ifdef DEBUG
          if (myconvDebug) {
            MyPrintf(STDERR_WITH_TIME,
                     "dmap_open(%s,%s,%d), CS=%d: iconv() returns %d, errno = %d in %s at %d\n",
                     to, from, idx, DMAP_S2S, len, errno, __FILE__,__LINE__);
            MyPrintf(STDERR_WITH_TIME,
                     "inBytesLeft = %d, inBuf - dmapSrc = %d\n", inBytesLeft, inBuf-dmapSrc);
            MyPrintf(STDERR_WITH_TIME,
                     "outBytesLeft = %d, outBuf - dmapS2S = %d\n", outBytesLeft, outBuf-(char *) dmap_rec[idx].dmapS2S);
          }
          if ((inBytesLeft == 86 || inBytesLeft == 64 || inBytesLeft == 1) &&
              memcmp(from, "IBM-1256", 9) == 0 &&
              memcmp(to, "IBM-420", 8) == 0) {
            /* Known problem for IBM-1256_IBM-420 */
            --inBytesLeft;
            ++inBuf;
            *outBuf=0x00;
            ++outBuf;
            --outBytesLeft;
            continue;
          } else if ((inBytesLeft == 173 || inBytesLeft == 172 ||
                      inBytesLeft == 74  || inBytesLeft == 73  ||
                      inBytesLeft == 52  || inBytesLeft == 50  ||
                      inBytesLeft == 31  || inBytesLeft == 20  ||
                      inBytesLeft == 6) &&
                     memcmp(to, "IBM-1256", 9) == 0 &&
                     memcmp(from, "IBM-420", 8) == 0) {
            /* Known problem for IBM-420_IBM-1256 */
            --inBytesLeft;
            ++inBuf;
            *outBuf=0x00;
            ++outBuf;
            --outBytesLeft;
            continue;
          } else if ((128 >= inBytesLeft) &&
                     memcmp(to, "IBM-037", 8) == 0 &&
                     memcmp(from, "IBM-367", 8) == 0) {
            /* Known problem for IBM-367_IBM-037 */
            --inBytesLeft;
            ++inBuf;
            *outBuf=0x00;
            ++outBuf;
            --outBytesLeft;
            continue;
          } else if (((1 <= inBytesLeft && inBytesLeft <= 4) || (97 <= inBytesLeft && inBytesLeft <= 128)) &&
                     memcmp(to, "IBM-838", 8) == 0 &&
                     memcmp(from, "TIS-620", 8) == 0) {
            /* Known problem for TIS-620_IBM-838 */
            --inBytesLeft;
            ++inBuf;
            *outBuf=0x00;
            ++outBuf;
            --outBytesLeft;
            continue;
          }
          iconv_close(cd);
          return -1;
#else
          /* Tolerant to undefined conversions for any converter */
          --inBytesLeft;
          ++inBuf;
          *outBuf=0x00;
          ++outBuf;
          --outBytesLeft;
          continue;
#endif
        }
      } while (inBytesLeft > 0);

      if (myconvIsISO(to))
        myconv_rec[idx].subS=0x1A;
      else if (myconvIsASCII(to))
        myconv_rec[idx].subS=0x7F;
      else if (myconvIsEBCDIC(to))
        myconv_rec[idx].subS=0x3F;

      if (myconvIsISO(from))
        myconv_rec[idx].srcSubS=0x1A;
      else if (myconvIsASCII(from))
        myconv_rec[idx].srcSubS=0x7F;
      else if (myconvIsEBCDIC(from))
        myconv_rec[idx].srcSubS=0x3F;

      iconv_close(cd);
    }
  } else if (((myconvIsSBCS(from) && myconvIsUnicode2(to)) && (dmap_rec[idx].codingSchema = DMAP_S2U)) ||
             ((myconvIsSBCS(from) && myconvIsUTF8(to)) && (dmap_rec[idx].codingSchema = DMAP_S28))) {
    int i;
    
    /* single byte mapping */
    if ((dmap_rec[idx].dmapD12U = (UniChar *) alloc_root(&dmapMemRoot, 0x100 * 2)) == NULL) {
#ifdef DEBUG
      MyPrintf(STDERR_WITH_TIME,
               "dmap_open(%s,%s,%d), CS=%d failed with malloc(), errno = %d in %s at %d\n",
               to, from, idx, DMAP_S2U, errno, __FILE__,__LINE__);
#endif
      return -1;
    }
    memset(dmap_rec[idx].dmapD12U, 0x00, 0x100 * 2);
    myconv_rec[idx].allocatedSize=0x100 * 2;

    
    {
      char      dmapSrc[2];
      iconv_t   cd;
      int32_t   i;
      size_t    inBytesLeft;
      size_t    outBytesLeft;
      size_t    len;
      char *    inBuf;
      char *    outBuf;
      char      SS=0x1A;
#ifdef support_surrogate
      if ((cd = iconv_open("UTF-16", from)) == (iconv_t) -1) {
#else
      if ((cd = iconv_open("UCS-2", from)) == (iconv_t) -1) {
#endif
#ifdef DEBUG
        MyPrintf(STDERR_WITH_TIME,
                 "dmap_open(%s,%s,%d) failed with iconv_open(), errno = %d in %s at %d\n",
                 to, from, idx, errno, __FILE__,__LINE__);
#endif
        return -1;
      }

      for (i = 0; i < 0x100; ++i) {
        dmapSrc[0]=i;
        inBuf=dmapSrc;
        inBytesLeft=1;
        outBuf=(char *) &(dmap_rec[idx].dmapD12U[i]);
        outBytesLeft=2;
        if ((len = iconv(cd, &inBuf, &inBytesLeft, &outBuf, &outBytesLeft)) != (size_t) 0) {
          if ((errno == EILSEQ || errno == EINVAL) &&
              inBytesLeft == 1 &&
              outBytesLeft == 2) {
            continue;
          } else {
#ifdef DEBUG
            if (myconvDebug) {
              MyPrintf(STDERR_WITH_TIME,
                       "dmap_open(%s,%s,%d) failed to initialize with iconv(cd,%02x,%d,%02x%02x,%d), errno = %d in %s at %d\n",
                       to, from, idx, dmapSrc[0], 1,
                       (&dmap_rec[idx].dmapD12U[i])[0],(&dmap_rec[idx].dmapD12U[i])[1], 2,
                       errno, __FILE__,__LINE__);
              MyPrintf(STDERR_WITH_TIME,
                       "inBytesLeft=%d, outBytesLeft=%d, %02x%02x\n",
                       inBytesLeft, outBytesLeft,
                       (&dmap_rec[idx].dmapD12U[i])[0],(&dmap_rec[idx].dmapD12U[i])[1]);
            }
#endif
            iconv_close(cd);
            return -1;
          }            
          dmap_rec[idx].dmapD12U[i]=0x0000;
        }
        if (dmap_rec[idx].dmapE02U[i] == 0x001A &&      /* pick the first one */
            myconv_rec[idx].srcSubS == 0x00) {
          myconv_rec[idx].srcSubS=i;
        }
      }
      iconv_close(cd);
    }
    myconv_rec[idx].subS=0x1A;
    myconv_rec[idx].subD=0xFFFD;
    

  } else if (((myconvIsUCS2(from)  && myconvIsSBCS(to)) && (dmap_rec[idx].codingSchema = DMAP_U2S)) ||
             ((myconvIsUTF16(from) && myconvIsSBCS(to)) && (dmap_rec[idx].codingSchema = DMAP_T2S)) ||
             ((myconvIsUTF8(from)  && myconvIsSBCS(to)) && (dmap_rec[idx].codingSchema = DMAP_82S))) {
    /* UTF-16 -> SBCS, the direct map a bit of waste of space,
     * binary search may be reasonable alternative
     */
    if ((dmap_rec[idx].dmapU2S = (uchar *) alloc_root(&dmapMemRoot, 0x10000 * 2)) == NULL) {
#ifdef DEBUG
        MyPrintf(STDERR_WITH_TIME,
                 "dmap_open(%s,%s,%d), CS=%d failed with malloc(), errno = %d in %s at %d\n",
                 to, from, idx, DMAP_U2S, errno, __FILE__,__LINE__);
#endif
      return -1;
    }
    memset(dmap_rec[idx].dmapU2S, 0x00, 0x10000);
    myconv_rec[idx].allocatedSize=(0x10000 * 2);

    {
      iconv_t   cd;
      int32_t   i;

#ifdef support_surrogate
      if ((cd = iconv_open(to, "UTF-16")) == (iconv_t) -1) {
#else
      if ((cd = iconv_open(to, "UCS-2")) == (iconv_t) -1) {
#endif
#ifdef DEBUG
        MyPrintf(STDERR_WITH_TIME,
                 "dmap_open(%s,%s,%d) failed with iconv_open(), errno = %d in %s at %d\n",
                 to, from, idx, errno, __FILE__,__LINE__);
#endif
        return -1;
      }

      for (i = 0; i < 0x100; ++i) {
        UniChar         dmapSrc[0x100];
        int32_t         j;
        for (j = 0; j < 0x100; ++j) {
          dmapSrc[j]=i * 0x100 + j;
        }
        char *  inBuf=(char *) dmapSrc;
        char *  outBuf=(char *) &(dmap_rec[idx].dmapU2S[i*0x100]);
        size_t  inBytesLeft=sizeof(dmapSrc);
        size_t  outBytesLeft=0x100;
        size_t  len;

        if ((len = iconv(cd, &inBuf, &inBytesLeft, &outBuf, &outBytesLeft)) != (size_t) 0) {
          if (inBytesLeft == 0 && outBytesLeft == 0) {    /* a number of substitution returns */
            continue;
          }
#ifdef DEBUG
          if (myconvDebug) {
            MyPrintf(STDERR_WITH_TIME,
                     "dmap_open(%s,%s,%d) failed to initialize with iconv(), errno = %d in %s at %d\n",
                     from, to, idx, errno, __FILE__,__LINE__);
            MyPrintf(STDERR_WITH_TIME,
                     "iconv() retuns %d, errno=%d, InBytesLeft=%d, OutBytesLeft=%d\n",
                     len, errno, inBytesLeft, outBytesLeft, __FILE__,__LINE__);
          }
#endif
          iconv_close(cd);
          return -1;
        }
      }
      iconv_close(cd);

      myconv_rec[idx].subS = dmap_rec[idx].dmapU2S[0x1A];
      myconv_rec[idx].subD = dmap_rec[idx].dmapU2S[0xFFFD];
      myconv_rec[idx].srcSubS = 0x1A;
      myconv_rec[idx].srcSubD = 0xFFFD;
    }



  } else if (((myconvIsDBCS(from) && myconvIsUnicode2(to)) && (dmap_rec[idx].codingSchema = DMAP_D2U)) ||
             ((myconvIsDBCS(from) && myconvIsUTF8(to)) && (dmap_rec[idx].codingSchema = DMAP_D28))) {
    int i;
    /* single byte mapping */
    if ((dmap_rec[idx].dmapD12U = (UniChar *) alloc_root(&dmapMemRoot, 0x100 * 2)) == NULL) {
#ifdef DEBUG
        MyPrintf(STDERR_WITH_TIME,
                 "dmap_open(%s,%s,%d), CS=%d failed with malloc(), errno = %d in %s at %d\n",
                 to, from, idx, DMAP_D2U, errno, __FILE__,__LINE__);
#endif
      return -1;
    }
    memset(dmap_rec[idx].dmapD12U, 0x00, 0x100 * 2);

    /* double byte mapping, assume 7 bit ASCII is not use as the first byte of DBCS. */
    if ((dmap_rec[idx].dmapD22U = (UniChar *) alloc_root(&dmapMemRoot, 0x8000 * 2)) == NULL) {
#ifdef DEBUG
        MyPrintf(STDERR_WITH_TIME,
                 "dmap_open(%s,%s,%d), CS=%d failed with malloc(), errno = %d in %s at %d\n",
                 to, from, idx, DMAP_D2U, errno, __FILE__,__LINE__);
#endif
      return -1;
    }
    memset(dmap_rec[idx].dmapD22U, 0x00, 0x8000 * 2);

    myconv_rec[idx].allocatedSize=(0x100 + 0x8000) * 2;

    
    {
      char      dmapSrc[2];
      iconv_t   cd;
      int32_t   i;
      size_t    inBytesLeft;
      size_t    outBytesLeft;
      size_t    len;
      char *    inBuf;
      char *    outBuf;
      char      SS=0x1A;

#ifdef support_surrogate
      if ((cd = iconv_open("UTF-16", from)) == (iconv_t) -1) {
#else
      if ((cd = iconv_open("UCS-2", from)) == (iconv_t) -1) {
#endif
#ifdef DEBUG
        MyPrintf(STDERR_WITH_TIME,
                 "dmap_open(%s,%s,%d) failed with iconv_open(), errno = %d in %s at %d\n",
                   to, from, idx, errno, __FILE__,__LINE__);
#endif
        return -1;
      }

      for (i = 0; i < 0x100; ++i) {
        dmapSrc[0]=i;
        inBuf=dmapSrc;
        inBytesLeft=1;
        outBuf=(char *) (&dmap_rec[idx].dmapD12U[i]);
        outBytesLeft=2;
        if ((len = iconv(cd, &inBuf, &inBytesLeft, &outBuf, &outBytesLeft)) != (size_t) 0) {
          if ((errno == EILSEQ || errno == EINVAL) &&
              inBytesLeft == 1 &&
              outBytesLeft == 2) {
            continue;
          } else {
#ifdef DEBUG
            if (myconvDebug) {
              MyPrintf(STDERR_WITH_TIME,
                       "dmap_open(%s,%s,%d) failed to initialize with iconv(cd,%02x,%d,%02x%02x,%d), errno = %d in %s at %d\n",
                       to, from, idx, dmapSrc[0], 1,
                       (&dmap_rec[idx].dmapD12U[i])[0],(&dmap_rec[idx].dmapD12U[i])[1], 2,
                       errno, __FILE__,__LINE__);
              MyPrintf(STDERR_WITH_TIME,
                       "inBytesLeft=%d, outBytesLeft=%d, %02x%02x\n",
                       inBytesLeft, outBytesLeft,
                       (&dmap_rec[idx].dmapD12U[i])[0],(&dmap_rec[idx].dmapD12U[i])[1]);
            }
#endif
            iconv_close(cd);
            return -1;
          }            
          dmap_rec[idx].dmapD12U[i]=0x0000;
        }
        if (dmap_rec[idx].dmapD12U[i] == 0x001A &&      /* pick the first one */
            myconv_rec[idx].srcSubS == 0x00) {
          myconv_rec[idx].srcSubS=i;
        }
      }

      
      for (i = 0x80; i < 0x100; ++i) {
        int j;
        if (dmap_rec[idx].dmapD12U[i] != 0x0000)
          continue;
        for (j = 0x01; j < 0x100; ++j) {
          dmapSrc[0]=i;
          dmapSrc[1]=j;
          int offset = i-0x80;
          offset<<=8;
          offset+=j;

          inBuf=dmapSrc;
          inBytesLeft=2;
          outBuf=(char *) &(dmap_rec[idx].dmapD22U[offset]);
          outBytesLeft=2;
          if ((len = iconv(cd, &inBuf, &inBytesLeft, &outBuf, &outBytesLeft)) != (size_t) 0) {
            if (inBytesLeft == 2 && outBytesLeft == 2 && (errno == EILSEQ || errno == EINVAL)) {
              ;  /* invalid DBCS character, dmapDD2U[offset] remains 0x0000 */
            } else {
#ifdef DEBUG
              if (myconvDebug) {
                MyPrintf(STDERR_WITH_TIME,
                         "dmap_open(%s,%s,%d) failed to initialize with iconv(cd,%p,2,%p,2), errno = %d in %s at %d\n",
                         to, from, idx,
                         dmapSrc, &(dmap_rec[idx].dmapD22U[offset]),
                         errno, __FILE__,__LINE__);
                MyPrintf(STDERR_WO_TIME, 
                         "iconv(cd,0x%02x%02x,2,0x%04x,2) returns %d, inBytesLeft=%d, outBytesLeft=%d\n",
                         dmapSrc[0], dmapSrc[1],
                         dmap_rec[idx].dmapD22U[offset],
                         len, inBytesLeft, outBytesLeft);
              }
#endif
              iconv_close(cd);
              return -1;
            }
          } else {
#ifdef TRACE_DMAP
            if (myconvDebug) {
              MyPrintf(STDERR_WITH_TIME,
                       "dmap_open(%s,%s,%d) failed to initialize with iconv(), rc=%d, errno=%d in %s at %d\n",
                       to, from, idx, len, errno, __FILE__,__LINE__);
              MyPrintf(STDERR_WITH_TIME,
                       "%04X: src=%04X%04X, inBuf=0x%02X%02X, inBytesLeft=%d, outBuf=%02X%02X%02X, outBytesLeft=%d\n",
                       i, dmapSrc[0], dmapSrc[1], inBuf[0], inBuf[1],
                       inBytesLeft, outBuf[-2], outBuf[-1], outBuf[0], outBytesLeft);
              MyPrintf(STDERR_WITH_TIME,
                       "&dmapSrc=%p, inBuf=%p, %p, outBuf=%p\n",
                       dmapSrc, inBuf, dmap_rec[idx].dmapU2M3 + (i - 0x80) * 2, outBuf);
            }
#endif
          }
        }
        if (dmap_rec[idx].dmapD12U[i] == 0xFFFD) {      /* pick the last one */
          myconv_rec[idx].srcSubD=i* 0x100 + j;
        }
      }
      iconv_close(cd);
    }

    myconv_rec[idx].subS=0x1A;
    myconv_rec[idx].subD=0xFFFD;
    myconv_rec[idx].srcSubD=0xFCFC;
    

  } else if (((myconvIsUCS2(from)  && myconvIsDBCS(to)) && (dmap_rec[idx].codingSchema = DMAP_U2D)) ||
             ((myconvIsUTF16(from) && myconvIsDBCS(to)) && (dmap_rec[idx].codingSchema = DMAP_T2D)) ||
             ((myconvIsUTF8(from)  && myconvIsDBCS(to)) && (dmap_rec[idx].codingSchema = DMAP_82D))) {
    /* UTF-16 -> DBCS single/double byte */
    /* A single table will cover all characters, assuming no second byte is 0x00. */
    if ((dmap_rec[idx].dmapU2D = (uchar *) alloc_root(&dmapMemRoot, 0x10000 * 2)) == NULL) {
#ifdef DEBUG
        MyPrintf(STDERR_WITH_TIME,
                 "dmap_open(%s,%s,%d), CS=%d failed with malloc(), errno = %d in %s at %d\n",
                 to, from, idx, DMAP_U2D, errno, __FILE__,__LINE__);
#endif
      return -1;
    }

    memset(dmap_rec[idx].dmapU2D, 0x00, 0x10000 * 2);
    myconv_rec[idx].allocatedSize=(0x10000 * 2);

    {
      UniChar   dmapSrc[1];
      iconv_t   cd;
      int32_t   i;
      size_t    inBytesLeft;
      size_t    outBytesLeft;
      size_t    len;
      char *    inBuf;
      char *    outBuf;

#ifdef support_surrogate
      if ((cd = iconv_open(to, "UTF-16")) == (iconv_t) -1) {
#else
      if ((cd = iconv_open(to, "UCS-2")) == (iconv_t) -1) {
#endif
#ifdef DEBUG
        MyPrintf(STDERR_WITH_TIME,
                 "dmap_open(%s,%s,%d) failed with iconv_open(), errno = %d in %s at %d\n",
                 to, from, idx, errno, __FILE__,__LINE__);
#endif
        return -1;
      }

      /* easy implementation, convert 1 Unicode character at one time. */
      /* If the open performance is an issue, convert a chunk such as 128 chracters.  */
      /* if the converted length is not the same as the original, convert one by one. */
      (dmap_rec[idx].dmapU2D)[0x0000]=0x00;
      for (i = 1; i < 0x10000; ++i) {
        dmapSrc[0]=i;
        inBuf=(char *) dmapSrc;
        inBytesLeft=2;
        outBuf=(char *) &((dmap_rec[idx].dmapU2D)[2*i]);
        outBytesLeft=2;
        do {
          if ((len = iconv(cd, &inBuf, &inBytesLeft, &outBuf, &outBytesLeft)) != (size_t) 0) {
            if (len == 1 && inBytesLeft == 0 && outBytesLeft == 1 && (dmap_rec[idx].dmapU2D)[2*i] == 0x1A) {
              /* UCS-2_TIS-620:0x0080 => 0x1A, converted to SBCS replacement character */
              (dmap_rec[idx].dmapU2D)[2*i+1]=0x00;
              break;
            } else if (len == 1 && inBytesLeft == 0 && outBytesLeft == 0) {
              break;
            }
            if (errno == EILSEQ || errno == EINVAL) {
#ifdef DEBUG
              if (myconvDebug) {
                MyPrintf(STDERR_WITH_TIME,
                         "dmap_open(%s,%s,%d) failed to initialize with iconv(), errno = %d in %s at %d\n",
                         to, from, idx, errno, __FILE__,__LINE__);
                MyPrintf(STDERR_WO_TIME,
                         "iconv(cd,%04x,2,%02x%02x,2) returns inBytesLeft=%d, outBytesLeft=%d\n",
                         dmapSrc[0],
                         (dmap_rec[idx].dmapU2D)[2*i], (dmap_rec[idx].dmapU2D)[2*i+1],
                         inBytesLeft, outBytesLeft);
                if (outBuf - (char *) dmap_rec[idx].dmapU2M2 > 1)
                  MyPrintf(STDERR_WO_TIME, "outBuf[-2..2]=%02X%02X%02X%02X%02X\n", outBuf[-2],outBuf[-1],outBuf[0],outBuf[1],outBuf[2]);
                else
                  MyPrintf(STDERR_WO_TIME, "outBuf[0..2]=%02X%02X%02X\n", outBuf[0],outBuf[1],outBuf[2]);
              }
#endif
              inBuf+=2;
              inBytesLeft-=2;
              memcpy(outBuf, (char *) &(myconv_rec[idx].subD), 2);
              outBuf+=2;
              outBytesLeft-=2;
            } else {
#ifdef DEBUG
              MyPrintf(STDERR_WITH_TIME,
                       "[%d] dmap_open(%s,%s,%d) failed to initialize with iconv(), errno = %d in %s at %d\n",
                       i, to, from, idx, errno, __FILE__,__LINE__);
              MyPrintf(STDERR_WO_TIME, 
                       "iconv(cd,%04x,2,%02x%02x,2) returns %d inBytesLeft=%d, outBytesLeft=%d\n",
                       dmapSrc[0],
                       (dmap_rec[idx].dmapU2D)[2*i],
                       (dmap_rec[idx].dmapU2D)[2*i+1],
                       len, inBytesLeft,outBytesLeft);
              if (i == 1) {
                MyPrintf(STDERR_WO_TIME, 
                         " inBuf [-1..2]=%02x%02x%02x%02x\n",
                         inBuf[-1],inBuf[0],inBuf[1],inBuf[2]);
                MyPrintf(STDERR_WO_TIME,
                         " outBuf [-1..2]=%02x%02x%02x%02x\n",
                         outBuf[-1],outBuf[0],outBuf[1],outBuf[2]);
              } else {
                MyPrintf(STDERR_WO_TIME, 
                         " inBuf [-2..2]=%02x%02x%02x%02x%02x\n",
                         inBuf[-2],inBuf[-1],inBuf[0],inBuf[1],inBuf[2]);
                MyPrintf(STDERR_WO_TIME,
                         " outBuf [-2..2]=%02x%02x%02x%02x%02x\n",
                         outBuf[-2],outBuf[-1],outBuf[0],outBuf[1],outBuf[2]);
              }
#endif
              iconv_close(cd);
              return -1;
            }
            if (len == 0 && inBytesLeft == 0 && outBytesLeft == 1) { /* converted to SBCS */
              (dmap_rec[idx].dmapU2D)[2*i+1]=0x00;
              break;
            }
          }
        } while (inBytesLeft > 0);
      }
      iconv_close(cd);
      myconv_rec[idx].subS = dmap_rec[idx].dmapU2D[2*0x1A];
      myconv_rec[idx].subD = dmap_rec[idx].dmapU2D[2*0xFFFD] * 0x100 
        + dmap_rec[idx].dmapU2D[2*0xFFFD+1];
      myconv_rec[idx].srcSubS = 0x1A;
      myconv_rec[idx].srcSubD = 0xFFFD;
    }


  } else if (((myconvIsEUC(from) && myconvIsUnicode2(to)) && (dmap_rec[idx].codingSchema = DMAP_E2U)) ||
             ((myconvIsEUC(from) && myconvIsUTF8(to)) && (dmap_rec[idx].codingSchema = DMAP_E28))) {
    int i;
    /* S0: 0x00 - 0x7F */
    if ((dmap_rec[idx].dmapE02U = (UniChar *) alloc_root(&dmapMemRoot, 0x100 * 2)) == NULL) {
#ifdef DEBUG
        MyPrintf(STDERR_WITH_TIME,
                 "dmap_open(%s,%s,%d), CS=%d failed with malloc(), errno = %d in %s at %d\n",
                 to, from, idx, DMAP_E2U, errno, __FILE__,__LINE__);
#endif
      return -1;
    }
    memset(dmap_rec[idx].dmapE02U, 0x00, 0x100 * 2);

    /* S1: 0xA0 - 0xFF, 0xA0 - 0xFF */
    if ((dmap_rec[idx].dmapE12U = (UniChar *) alloc_root(&dmapMemRoot, 0x60 * 0x60 * 2)) == NULL) {
#ifdef DEBUG
        MyPrintf(STDERR_WITH_TIME,
                 "dmap_open(%s,%s,%d), CS=%d failed with malloc(), errno = %d in %s at %d\n",
                 to, from, idx, DMAP_E2U, errno, __FILE__,__LINE__);
#endif
      return -1;
    }
    memset(dmap_rec[idx].dmapE12U, 0x00, 0x60 * 0x60 * 2);

    /* SS2: 0x8E + 0xA0 - 0xFF, 0xA0 - 0xFF */
    if ((dmap_rec[idx].dmapE22U = (UniChar *) alloc_root(&dmapMemRoot, 0x60 * 0x61 * 2)) == NULL) {
#ifdef DEBUG
        MyPrintf(STDERR_WITH_TIME,
                 "dmap_open(%s,%s,%d), CS=%d failed with malloc(), errno = %d in %s at %d\n",
                 to, from, idx, DMAP_E2U, errno, __FILE__,__LINE__);
#endif
      return -1;
    }
    memset(dmap_rec[idx].dmapE22U, 0x00, 0x60 * 0x61 * 2);

    /* SS3: 0x8F + 0xA0 - 0xFF, 0xA0 - 0xFF */
    if ((dmap_rec[idx].dmapE32U = (UniChar *) alloc_root(&dmapMemRoot, 0x60 * 0x61 * 2)) == NULL) {
#ifdef DEBUG
        MyPrintf(STDERR_WITH_TIME,
                 "dmap_open(%s,%s,%d), CS=%d failed with malloc(), errno = %d in %s at %d\n",
                 to, from, idx, DMAP_E2U, errno, __FILE__,__LINE__);
#endif
      return -1;
    }
    memset(dmap_rec[idx].dmapE32U, 0x00, 0x60 * 0x61 * 2);

    myconv_rec[idx].allocatedSize=(0x100 + 0x60 * 0x60  + 0x60 * 0x61* 2) * 2;

    
    {
      char      dmapSrc[0x60 * 0x60 * 3];
      iconv_t   cd;
      int32_t   i;
      size_t    inBytesLeft;
      size_t    outBytesLeft;
      size_t    len;
      char *    inBuf;
      char *    outBuf;
      char      SS=0x8E;

#ifdef support_surrogate
      if ((cd = iconv_open("UTF-16", from)) == (iconv_t) -1) {
#else
      if ((cd = iconv_open("UCS-2", from)) == (iconv_t) -1) {
#endif
#ifdef DEBUG
        MyPrintf(STDERR_WITH_TIME,
                 "dmap_open(%s,%s,%d) failed with iconv_open(), errno = %d in %s at %d\n",
                   to, from, idx, errno, __FILE__,__LINE__);
#endif
        return -1;
      }

      for (i = 0; i < 0x100; ++i) {
        dmapSrc[0]=i;
        inBuf=dmapSrc;
        inBytesLeft=1;
        outBuf=(char *) (&dmap_rec[idx].dmapE02U[i]);
        outBytesLeft=2;
        if ((len = iconv(cd, &inBuf, &inBytesLeft, &outBuf, &outBytesLeft)) != (size_t) 0) {
#ifdef DEBUG
          if (myconvDebug) {
            MyPrintf(STDERR_WITH_TIME,
                     "dmap_open(%s,%s,%d) failed to initialize with iconv(), errno = %d in %s at %d\n",
                     to, from, idx, errno, __FILE__,__LINE__);
          }
#endif
          dmap_rec[idx].dmapE02U[i]=0x0000;
        }
        if (dmap_rec[idx].dmapE02U[i] == 0x001A &&      /* pick the first one */
            myconv_rec[idx].srcSubS == 0x00) {
          myconv_rec[idx].srcSubS=i;
        }
      }

      
      inBuf=dmapSrc;
      for (i = 0; i < 0x60; ++i) {
        int j;
        for (j = 0; j < 0x60; ++j) {
          *inBuf=i+0xA0;
          ++inBuf;
          *inBuf=j+0xA0;
          ++inBuf;
        }
      }
      inBuf=dmapSrc;
      inBytesLeft=0x60 * 0x60 * 2;
      outBuf=(char *) dmap_rec[idx].dmapE12U;
      outBytesLeft=0x60 * 0x60 * 2;
      do {
        if ((len = iconv(cd, &inBuf, &inBytesLeft, &outBuf, &outBytesLeft)) != (size_t) 0) {
          if (errno == EILSEQ) {
#ifdef DEBUG
            if (myconvDebug) {
              MyPrintf(STDERR_WITH_TIME,
                       "dmap_open(%s,%s,%d) failed to initialize with iconv(), errno = %d in %s at %d\n",
                       to, from, idx, errno, __FILE__,__LINE__);
              MyPrintf(STDERR_WO_TIME, "inBytesLeft=%d, outBytesLeft=%d\n", inBytesLeft, outBytesLeft);
              if (inBuf - dmapSrc > 1 && inBuf - dmapSrc <= sizeof(dmapSrc) - 2)
                MyPrintf(STDERR_WO_TIME, "inBuf[-2..2]=%02X%02X%02X%02X%02X\n", inBuf[-2],inBuf[-1],inBuf[0],inBuf[1],inBuf[2]);
              else
                MyPrintf(STDERR_WO_TIME, "inBuf[0..2]=%02X%02X%02X\n", inBuf[0],inBuf[1],inBuf[2]);
              if (outBuf - (char *) dmap_rec[idx].dmapE12U > 1)
                MyPrintf(STDERR_WO_TIME, "outBuf[-2..2]=%02X%02X%02X%02X%02X\n", outBuf[-2],outBuf[-1],outBuf[0],outBuf[1],outBuf[2]);
              else
                MyPrintf(STDERR_WO_TIME, "outBuf[0..2]=%02X%02X%02X\n", outBuf[0],outBuf[1],outBuf[2]);
            }
#endif
            inBuf+=2;
            inBytesLeft-=2;
            outBuf[0]=0x00;
            outBuf[1]=0x00;
            outBuf+=2;
            outBytesLeft-=2;
          } else {
#ifdef DEBUG
            MyPrintf(STDERR_WITH_TIME,
                     "dmap_open(%s,%s,%d) failed to initialize with iconv(), errno = %d in %s at %d\n",
                     to, from, idx, errno, __FILE__,__LINE__);
#endif
            iconv_close(cd);
            return -1;
          }
        }
      } while (inBytesLeft > 0);

      /* SS2: 0x8E + 1 or 2 bytes */
      /* SS3: 0x8E + 1 or 2 bytes */
      while (SS != 0x00) {
        int32_t numSuccess=0;
        for (i = 0; i < 0x60; ++i) {
          inBuf=dmapSrc;
          inBuf[0]=SS;
          inBuf[1]=i+0xA0;
          inBytesLeft=2;
          if (SS == 0x8E)
            outBuf=(char *) &(dmap_rec[idx].dmapE22U[i]);
          else
            outBuf=(char *) &(dmap_rec[idx].dmapE32U[i]);
          outBytesLeft=2;
          if ((len = iconv(cd, &inBuf, &inBytesLeft, &outBuf, &outBytesLeft)) != (size_t) 0) {
            if (SS == 0x8E)
              dmap_rec[idx].dmapE22U[i]=0x0000;
            else
              dmap_rec[idx].dmapE32U[i]=0x0000;
          } else {
            ++numSuccess;
          }
        }
        if (numSuccess == 0) { /* SS2 is 2 bytes */
          inBuf=dmapSrc;
          for (i = 0; i < 0x60; ++i) {
            int j;
            for (j = 0; j < 0x60; ++j) {
              *inBuf=SS;
              ++inBuf;
              *inBuf=i+0xA0;
              ++inBuf;
              *inBuf=j+0xA0;
              ++inBuf;
            }
          }
          inBuf=dmapSrc;
          inBytesLeft=0x60 * 0x60 * 3;
          if (SS == 0x8E)
            outBuf=(char *) &(dmap_rec[idx].dmapE22U[0x60]);
          else
            outBuf=(char *) &(dmap_rec[idx].dmapE32U[0x60]);
          outBytesLeft=0x60 * 0x60 * 2;
          do {
            if ((len = iconv(cd, &inBuf, &inBytesLeft, &outBuf, &outBytesLeft)) != (size_t) 0) {
#ifdef DEBUG
              if (myconvDebug) {
                MyPrintf(STDERR_WITH_TIME,
                         "%02X:dmap_open(%s,%s,%d) failed to initialize with iconv(), errno = %d in %s at %d\n",
                         SS, to, from, idx, errno, __FILE__,__LINE__);
                MyPrintf(STDERR_WO_TIME, "inBytesLeft=%d, outBytesLeft=%d\n", inBytesLeft, outBytesLeft);
                if (inBuf - dmapSrc > 1 && inBuf - dmapSrc <= sizeof(dmapSrc) - 2)
                  MyPrintf(STDERR_WO_TIME, "inBuf[-2..2]=%02X%02X%02X%02X%02X\n", inBuf[-2],inBuf[-1],inBuf[0],inBuf[1],inBuf[2]);
                else
                  MyPrintf(STDERR_WO_TIME, "inBuf[0..2]=%02X%02X%02X\n", inBuf[0],inBuf[1],inBuf[2]);
              }
#endif
              if (errno == EILSEQ || errno == EINVAL) {
                inBuf+=3;
                inBytesLeft-=3;
                outBuf[0]=0x00;
                outBuf[1]=0x00;
                outBuf+=2;
                outBytesLeft-=2;
              } else {
#ifdef DEBUG
                MyPrintf(STDERR_WITH_TIME,
                         "%02X:dmap_open(%s,%s,%d) failed to initialize with iconv(), errno = %d in %s at %d\n",
                         SS, to, from, idx, errno, __FILE__,__LINE__);
#endif
                iconv_close(cd);
                return -1;
              }
            }
          } while (inBytesLeft > 0);
        }
        if (SS == 0x8E)
          SS=0x8F;
        else 
          SS = 0x00;
      }
      iconv_close(cd);

      myconv_rec[idx].subS=0x1A;
      myconv_rec[idx].subD=0xFFFD;
      for (i = 0; i < 0x80; ++i) {
        if (dmap_rec[idx].dmapE02U[i] == 0x001A) {
          myconv_rec[idx].srcSubS=i;    /* pick the first one */
          break;
        }
      }

      for (i = 0; i < 0x60 * 0x60; ++i) {
        if (dmap_rec[idx].dmapE12U[i] == 0xFFFD) {
          uchar byte1=i / 0x60;
          uchar byte2=i % 0x60;
          myconv_rec[idx].srcSubD=(byte1 + 0xA0) * 0x100 + (byte2 + 0xA0);    /* pick the last one */
        }
      }

    }

  } else if (((myconvIsUCS2(from)  && myconvIsEUC(to)) && (dmap_rec[idx].codingSchema = DMAP_U2E)) ||
             ((myconvIsUTF16(from) && myconvIsEUC(to)) && (dmap_rec[idx].codingSchema = DMAP_T2E)) ||
             ((myconvIsUTF8(from)  && myconvIsEUC(to)) && (dmap_rec[idx].codingSchema = DMAP_82E))) {
    /* S0: 0x00 - 0xFF */
    if ((dmap_rec[idx].dmapU2S = (uchar *) alloc_root(&dmapMemRoot, 0x100)) == NULL) {
#ifdef DEBUG
        MyPrintf(STDERR_WITH_TIME,
                 "dmap_open(%s,%s,%d), CS=%d failed with malloc(), errno = %d in %s at %d\n",
                 to, from, idx, DMAP_U2E, errno, __FILE__,__LINE__);
#endif
      return -1;
    }
    memset(dmap_rec[idx].dmapU2S, 0x00, 0x100);

    /* U0080 - UFFFF -> S1: 0xA0 - 0xFF, 0xA0 - 0xFF */
    if ((dmap_rec[idx].dmapU2M2 = (uchar *) alloc_root(&dmapMemRoot, 0xFF80 * 2)) == NULL) {
#ifdef DEBUG
        MyPrintf(STDERR_WITH_TIME,
                 "dmap_open(%s,%s,%d), CS=%d failed with malloc(), errno = %d in %s at %d\n",
                 to, from, idx, DMAP_U2E, errno, __FILE__,__LINE__);
#endif
      return -1;
    }
    memset(dmap_rec[idx].dmapU2M2, 0x00, 0xFF80 * 2);

    /* U0080 - UFFFF -> SS2: 0x8E + 0xA0 - 0xFF, 0xA0 - 0xFF
     *                  SS3: 0x8F + 0xA0 - 0xFF, 0xA0 - 0xFF */
    if ((dmap_rec[idx].dmapU2M3 = (uchar *) alloc_root(&dmapMemRoot, 0xFF80 * 3)) == NULL) {
#ifdef DEBUG
      MyPrintf(STDERR_WITH_TIME,
               "dmap_open(%s,%s,%d), CS=%d failed with malloc(), errno = %d in %s at %d\n",
               to, from, idx, DMAP_U2E, errno, __FILE__,__LINE__);
#endif
      return -1;
    }
    memset(dmap_rec[idx].dmapU2M3, 0x00, 0xFF80 * 3);
    myconv_rec[idx].allocatedSize=(0x100 + 0xFF80 * 2 + 0xFF80 * 3);

    {
      UniChar   dmapSrc[0x80];
      iconv_t   cd;
      int32_t   i;
      size_t    inBytesLeft;
      size_t    outBytesLeft;
      size_t    len;
      char *    inBuf;
      char *    outBuf;

#ifdef support_surrogate
      if ((cd = iconv_open(to, "UTF-16")) == (iconv_t) -1) {
#else
      if ((cd = iconv_open(to, "UCS-2")) == (iconv_t) -1) {
#endif
#ifdef DEBUG
        MyPrintf(STDERR_WITH_TIME,
                 "dmap_open(%s,%s,%d) failed with iconv_open(), errno = %d in %s at %d\n",
                   to, from, idx, errno, __FILE__,__LINE__);
#endif
        return -1;
      }

      for (i = 0; i < 0x80; ++i)
        dmapSrc[i]=i;
      inBuf=(char *) dmapSrc;
      inBytesLeft=0x80 * 2;
      outBuf=(char *) dmap_rec[idx].dmapU2S;
      outBytesLeft=0x80;
      do {
        if ((len = iconv(cd, &inBuf, &inBytesLeft, &outBuf, &outBytesLeft)) != (size_t) 0) {
#ifdef DEBUG
          MyPrintf(STDERR_WITH_TIME,
                   "dmap_open(%s,%s,%d) failed to initialize with iconv(), errno = %d in %s at %d\n",
                   to, from, idx, errno, __FILE__,__LINE__);
#endif
          iconv_close(cd);
          return -1;
        }
      } while (inBytesLeft > 0);

      myconv_rec[idx].srcSubS = 0x1A;
      myconv_rec[idx].srcSubD = 0xFFFD;
      myconv_rec[idx].subS = dmap_rec[idx].dmapU2S[0x1A];
      
      outBuf=(char *) &(myconv_rec[idx].subD);
      dmapSrc[0]=0xFFFD;
      inBuf=(char *) dmapSrc;
      inBytesLeft=2;
      outBytesLeft=2;
      if ((len = iconv(cd, &inBuf, &inBytesLeft, &outBuf, &outBytesLeft)) != (size_t) 0) {
#ifdef DEBUG
        if (myconvDebug) {
          MyPrintf(STDERR_WITH_TIME,
                   "dmap_open(%s,%s,%d) failed to initialize with iconv(), rc=%d, errno=%d in %s at %d\n",
                   to, from, idx, len, errno, __FILE__,__LINE__);
          MyPrintf(STDERR_WO_TIME, "iconv(0x1A,1,%p,1) returns outBuf=%p, outBytesLeft=%d\n",
                   dmapSrc, outBuf, outBytesLeft);
        }
#endif
        if (outBytesLeft == 0) {
          /* UCS-2_IBM-eucKR returns error.
             myconv(iconv) rc=1, error=0, InBytesLeft=0, OutBytesLeft=18
             myconv(iconvRev) rc=-1, error=116, InBytesLeft=2, OutBytesLeft=20
             iconv: 0xFFFD => 0xAFFE => 0x    rc=1,-1  sub=0,0
          */
          ;
        } else {
          iconv_close(cd);
          return -1;
        }
      }
      
      for (i = 0x80; i < 0xFFFF; ++i) {
        uchar   eucBuf[3];
        dmapSrc[0]=i;
        inBuf=(char *) dmapSrc;
        inBytesLeft=2;
        outBuf=(char *) eucBuf;
        outBytesLeft=sizeof(eucBuf);
        errno=0;
        if ((len = iconv(cd, &inBuf, &inBytesLeft, &outBuf, &outBytesLeft)) != (size_t) 0) {
          if (len == 1 && errno == 0 && inBytesLeft == 0 && outBytesLeft == 1) {  /* substitution occurred. */            continue;
          }

          if (errno == EILSEQ) {
#ifdef DEBUG
            if (myconvDebug) {
              MyPrintf(STDERR_WITH_TIME,
                       "dmap_open(%s,%s,%d) failed to initialize with iconv(), errno = %d in %s at %d\n",
                       to, from, idx, errno, __FILE__,__LINE__);
              MyPrintf(STDERR_WO_TIME, "inBytesLeft=%d, outBytesLeft=%d\n", inBytesLeft, outBytesLeft);
              if (inBuf - (char *) dmapSrc > 1 && inBuf - (char *) dmapSrc <= sizeof(dmapSrc) - 2)
                MyPrintf(STDERR_WO_TIME, "inBuf[-2..2]=%02X%02X%02X%02X%02X\n", inBuf[-2],inBuf[-1],inBuf[0],inBuf[1],inBuf[2]);
              else
                MyPrintf(STDERR_WO_TIME, "inBuf[0..2]=%02X%02X%02X\n", inBuf[0],inBuf[1],inBuf[2]);
              if (outBuf - (char *) dmap_rec[idx].dmapU2M2 > 1)
                MyPrintf(STDERR_WO_TIME, "outBuf[-2..2]=%02X%02X%02X%02X%02X\n", outBuf[-2],outBuf[-1],outBuf[0],outBuf[1],outBuf[2]);
              else
                MyPrintf(STDERR_WO_TIME, "outBuf[0..2]=%02X%02X%02X\n", outBuf[0],outBuf[1],outBuf[2]);
            }
#endif
            inBuf+=2;
            inBytesLeft-=2;
            memcpy(outBuf, (char *) &(myconv_rec[idx].subD), 2);
            outBuf+=2;
            outBytesLeft-=2;
          } else {
#ifdef DEBUG
            if (myconvDebug) {
              MyPrintf(STDERR_WITH_TIME,
                       "dmap_open(%s,%s,%d) failed to initialize with iconv(), rc = %d, errno = %d in %s at %d\n",
                       to, from, idx, len, errno, __FILE__,__LINE__);
              MyPrintf(STDERR_WITH_TIME,
                       "%04X: src=%04X%04X, inBuf=0x%02X%02X, inBytesLeft=%d, outBuf[-2..0]=%02X%02X%02X, outBytesLeft=%d\n",
                       i, dmapSrc[0], dmapSrc[1], inBuf[0], inBuf[1],
                       inBytesLeft, outBuf[-2], outBuf[-1], outBuf[0], outBytesLeft);
              MyPrintf(STDERR_WITH_TIME,
                       "&dmapSrc=%p, inBuf=%p, dmapU2M2 + %d = %p, outBuf=%p\n",
                       dmapSrc, inBuf, (i - 0x80) * 2, dmap_rec[idx].dmapU2M2 + (i - 0x80) * 2, outBuf);
            }
#endif
            iconv_close(cd);
            return -1;
          }
        }
        if (sizeof(eucBuf) - outBytesLeft == 1) {
          if (i < 0x100) {
            (dmap_rec[idx].dmapU2S)[i]=eucBuf[0];
          } else {
            dmap_rec[idx].dmapU2M2[(i - 0x80) * 2]        = eucBuf[0];
            dmap_rec[idx].dmapU2M2[(i - 0x80) * 2 + 1]    = 0x00;
          }
        } else if (sizeof(eucBuf) - outBytesLeft == 2) {  /* 2 bytes */
          dmap_rec[idx].dmapU2M2[(i - 0x80) * 2]        = eucBuf[0];
          dmap_rec[idx].dmapU2M2[(i - 0x80) * 2 + 1]    = eucBuf[1];
        } else if (sizeof(eucBuf) - outBytesLeft == 3) { /* 3 byte SS2/SS3 */
          dmap_rec[idx].dmapU2M3[(i - 0x80) * 3]        = eucBuf[0];
          dmap_rec[idx].dmapU2M3[(i - 0x80) * 3 + 1]    = eucBuf[1];
          dmap_rec[idx].dmapU2M3[(i - 0x80) * 3 + 2]    = eucBuf[2];
        } else {
#ifdef DEBUG
          if (myconvDebug) {
            MyPrintf(STDERR_WITH_TIME,
                     "dmap_open(%s,%s,%d) failed to initialize with iconv(), rc=%d, errno=%d in %s at %d\n",
                     to, from, idx, len, errno, __FILE__,__LINE__);
            MyPrintf(STDERR_WITH_TIME,
                     "%04X: src=%04X%04X, inBuf=0x%02X%02X, inBytesLeft=%d, outBuf=%02X%02X%02X, outBytesLeft=%d\n",
                     i, dmapSrc[0], dmapSrc[1], inBuf[0], inBuf[1],
                     inBytesLeft, outBuf[-2], outBuf[-1], outBuf[0], outBytesLeft);
            MyPrintf(STDERR_WITH_TIME,
                     "&dmapSrc=%p, inBuf=%p, %p, outBuf=%p\n",
                     dmapSrc, inBuf, dmap_rec[idx].dmapU2M3 + (i - 0x80) * 2, outBuf);
          }
#endif
          return -1;
        }
          
      }
      iconv_close(cd);
    }

  } else if (myconvIsUTF16(from) && myconvIsUTF8(to)) {
    dmap_rec[idx].codingSchema = DMAP_T28;

  } else if (myconvIsUCS2(from) && myconvIsUTF8(to)) {
    dmap_rec[idx].codingSchema = DMAP_U28;

  } else if (myconvIsUTF8(from) && myconvIsUnicode2(to)) {
    dmap_rec[idx].codingSchema = DMAP_82U;

  } else if (myconvIsUnicode2(from) && myconvIsUnicode2(to)) {
    dmap_rec[idx].codingSchema = DMAP_U2U;

  } else {
    
    return -1;
  }
  myconv_rec[idx].cnv_dmap=&(dmap_rec[idx]);
  return 0;
}



static int      bins_open(const char *  to,
                          const char *  from,
                          const int32_t idx)
{
  return -1;
}



static int32_t  dmap_close(const int32_t        idx)
{
  if (dmap_rec[idx].codingSchema == DMAP_S2S) {
    if (dmap_rec[idx].dmapS2S != NULL) {
      dmap_rec[idx].dmapS2S=NULL;
    }
  } else if (dmap_rec[idx].codingSchema = DMAP_E2U) {
    if (dmap_rec[idx].dmapE02U != NULL) {
      dmap_rec[idx].dmapE02U=NULL;
    }
    if (dmap_rec[idx].dmapE12U != NULL) {
      dmap_rec[idx].dmapE12U=NULL;
    }
    if (dmap_rec[idx].dmapE22U != NULL) {
      dmap_rec[idx].dmapE22U=NULL;
    }
    if (dmap_rec[idx].dmapE32U != NULL) {
      dmap_rec[idx].dmapE32U=NULL;
    }
  }

  return 0;
}


static int32_t  bins_close(const int32_t        idx)
{
  return 0;
}


myconv_t myconv_open(const char *       toCode,
                     const char *       fromCode,
                     int32_t            converter)
{
  int32 i;
  for (i = 0; i < MAX_CONVERTER; ++i) {
    if (myconv_rec[i].converterType == 0)
      break;
  }
  if (i >= MAX_CONVERTER)
    return ((myconv_t) -1);

  myconv_rec[i].converterType = converter;
  myconv_rec[i].index=i;
  myconv_rec[i].fromCcsid=cstoccsid(fromCode);
  if (myconv_rec[i].fromCcsid == 0 && memcmp(fromCode, "big5",5) == 0)
    myconv_rec[i].fromCcsid=950;
  myconv_rec[i].toCcsid=cstoccsid(toCode);
  if (myconv_rec[i].toCcsid == 0 && memcmp(toCode, "big5",5) == 0)
    myconv_rec[i].toCcsid=950;
  strncpy(myconv_rec[i].from,   fromCode,       sizeof(myconv_rec[i].from)-1);
  strncpy(myconv_rec[i].to,     toCode,         sizeof(myconv_rec[i].to)-1);

  if (converter == CONVERTER_ICONV) {
    if ((myconv_rec[i].cnv_iconv=iconv_open(toCode, fromCode)) == (iconv_t) -1) {
      return ((myconv_t) -1);
    }
    myconv_rec[i].allocatedSize = -1;
    myconv_rec[i].srcSubS=myconvGetSubS(fromCode);
    myconv_rec[i].srcSubD=myconvGetSubD(fromCode);
    myconv_rec[i].subS=myconvGetSubS(toCode);
    myconv_rec[i].subD=myconvGetSubD(toCode);
    return &(myconv_rec[i]);
  } else if (converter == CONVERTER_DMAP &&
             dmap_open(toCode, fromCode, i) != -1) {
    return &(myconv_rec[i]);
  }
  return ((myconv_t) -1);
}



int32_t myconv_close(myconv_t cd)
{
  int32_t   ret=0;

  if (cd->converterType == CONVERTER_ICONV) {
    ret=iconv_close(cd->cnv_iconv);
  } else if (cd->converterType == CONVERTER_DMAP) {
    ret=dmap_close(cd->index);
  }
  memset(&(myconv_rec[cd->index]), 0x00, sizeof(myconv_rec[cd->index]));
  return ret;
}




/* reference: http://www-306.ibm.com/software/globalization/other/es.jsp */
/* systemCL would be expensive, and myconvIsXXXXX is called frequently.
   need to cache entries */
#define MAX_CCSID       256
static int      ccsidList      [MAX_CCSID];
static int      esList         [MAX_CCSID];
int32 getEncodingScheme(const uint16 inCcsid, int32& outEncodingScheme);
EXTERN int      myconvGetES(CCSID     ccsid) 
{
  /* call QtqValidateCCSID in ILE to get encoding schema */
  /*  return QtqValidateCCSID(ccsid); */
  int   i;
  for (i = 0; i < MAX_CCSID; ++i) {
    if (ccsidList[i] == ccsid)
      return esList[i];
    if (ccsidList[i] == 0x00)
      break;
  }

  if (i >= MAX_CCSID) {
    i=MAX_CCSID-1;
  }

  { 
    ccsidList[i]=ccsid;
    getEncodingScheme(ccsid, esList[i]);
#ifdef DEBUG_PASE
    if (myconvDebug) {
      fprintf(stderr, "CCSID=%d, ES=0x%04X\n", ccsid, esList[i]);
    }
#endif
    return esList[i];
  }
  return 0;
}


EXTERN  int     myconvIsEBCDIC(const char *     pName)
{
  int   es = myconvGetES(cstoccsid(pName));
  if (es == 0x1100 ||
      es == 0x1200 ||
      es == 0x6100 ||
      es == 0x6200 ||
      es == 0x1301 ) {
    return TRUE;
  }
  return FALSE;
}


EXTERN int      myconvIsISO(const char *    pName)
{
  int   es = myconvGetES(cstoccsid(pName));
  if (es == 0x4100 ||
      es == 0x4105 ||
      es == 0x4155 ||
      es == 0x5100 ||
      es == 0x5150 ||
      es == 0x5200 ||
      es == 0x5404 ||
      es == 0x5409 ||
      es == 0x540A ||
      es == 0x5700) {
    return TRUE;
  }
  return FALSE;
}


EXTERN  int     myconvIsASCII(const char *      pName)
{
  int   es = myconvGetES(cstoccsid(pName));
  if (es == 0x2100 ||
      es == 0x3100 ||
      es == 0x8100 ||
      es == 0x2200 ||
      es == 0x3200 ||
      es == 0x9200 ||
      es == 0x2300 ||
      es == 0x2305 ||
      es == 0x3300 ||
      es == 0x2900 ||
      es == 0x2A00) {
    return TRUE;
  } else if (memcmp(pName, "big5", 5) == 0) {
    return TRUE;
  }
  return FALSE;
}



EXTERN  int     myconvIsUCS2(const char *       pName)
{
  if (cstoccsid(pName) == 13488) {
    return TRUE;
  }
  return FALSE;
}


EXTERN  int     myconvIsUTF16(const char *       pName)
{
  if (cstoccsid(pName) == 1200) {
    return TRUE;
  }
  return FALSE;
}


EXTERN  int     myconvIsUnicode2(const char *       pName)
{
  int   es = myconvGetES(cstoccsid(pName));
  if (es == 0x7200 ||
      es == 0x720B ||
      es == 0x720F) {
    return TRUE;
  }
  return FALSE;
}


EXTERN  int     myconvIsUTF8(const char *       pName)
{
  int   es = myconvGetES(cstoccsid(pName));
  if (es == 0x7807) {
    return TRUE;
  }
  return FALSE;
}


EXTERN  int     myconvIsUnicode(const char *       pName)
{
  int   es = myconvGetES(cstoccsid(pName));
  if (es == 0x7200 ||
      es == 0x720B ||
      es == 0x720F ||
      es == 0x7807) {
    return TRUE;
  }
  return FALSE;
}


EXTERN  int     myconvIsEUC(const char *        pName)
{
  int   es = myconvGetES(cstoccsid(pName));
  if (es == 0x4403) {
    return TRUE;
  }
  return FALSE;
}


EXTERN int      myconvIsDBCS(const char *   pName)
{
  int   es = myconvGetES(cstoccsid(pName));
  if (es == 0x1200 ||
      es == 0x2200 ||
      es == 0x2300 ||
      es == 0x2305 ||
      es == 0x2A00 ||
      es == 0x3200 ||
      es == 0x3300 ||
      es == 0x5200 ||
      es == 0x6200 ||
      es == 0x9200) {
    return TRUE;
  } else if (memcmp(pName, "big5", 5) == 0) {
    return TRUE;
  }
  return FALSE;
}


EXTERN int      myconvIsSBCS(const char *   pName)
{
  int   es = myconvGetES(cstoccsid(pName));
  if (es == 0x1100 ||
      es == 0x2100 ||
      es == 0x3100 ||
      es == 0x4100 ||
      es == 0x4105 ||
      es == 0x5100 ||
      es == 0x5150 ||
      es == 0x6100 ||
      es == 0x8100) {
    return TRUE;
  }
  return FALSE;
}



EXTERN char myconvGetSubS(const char *     code) 
{
  if (myconvIsEBCDIC(code)) {
    return 0x3F;
  } else if (myconvIsASCII(code)) {
    return 0x1A;
  } else if (myconvIsISO(code)) {
    return 0x1A;
  } else if (myconvIsEUC(code)) {
    return 0x1A;
  } else if (myconvIsUCS2(code)) {
    return 0x00;
  } else if (myconvIsUTF8(code)) {
    return 0x1A;
  }
  return 0x00;
}


EXTERN UniChar myconvGetSubD(const char *     code) 
{
  if (myconvIsEBCDIC(code)) {
    return 0xFDFD;
  } else if (myconvIsASCII(code)) {
    return 0xFCFC;
  } else if (myconvIsISO(code)) {
    return 0x00;
  } else if (myconvIsEUC(code)) {
    return 0x00;
  } else if (myconvIsUCS2(code)) {
    return 0xFFFD;
  } else if (myconvIsUTF8(code)) {
    return 0x00;
  }
  return 0x00;
}

