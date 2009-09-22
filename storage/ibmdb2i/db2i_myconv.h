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

#ifndef DB2I_MYCONV_H
#define DB2I_MYCONV_H


#include <sys/time.h>
#include <stdlib.h>
#include <stdio.h>
#include <wchar.h>
#include <errno.h>
#include <iconv.h>
#include <ctype.h>
#include <time.h>
#include <stdarg.h>
#include <string.h>

#ifndef TRUE
#define TRUE	1
#endif

#ifndef FALSE
#define FALSE	0
#endif

#ifdef __cplusplus
#define INTERN  inline
#define EXTERN  extern "C"
#else
#define INTERN  static
#define EXTERN  extern
#endif


/* ANSI integer data types */
#if defined(__OS400_TGTVRM__)
/* for DTAMDL(*P128), datamodel(P128): int/long/pointer=4/4/16 */
/* LLP64:4/4/8 is used for teraspace ?? */
typedef short                   int16_t;
typedef unsigned short          uint16_t;
typedef int                     int32_t;
typedef unsigned int            uint32_t;
typedef long long               int64_t;
typedef unsigned long long      uint64_t;
#elif defined(PASE)
/* PASE uses IPL32: int/long/pointer=4/4/4 + long long */
#elif defined(__64BIT__)
/* AIX 64 bit uses LP64: int/long/pointer=4/8/8 */
#endif

#define CONVERTER_ICONV         1
#define CONVERTER_DMAP          2

#define DMAP_S2S                10
#define DMAP_S2U                20
#define DMAP_D2U                30
#define DMAP_E2U                40
#define DMAP_U2S                120
#define DMAP_T2S                125
#define DMAP_U2D                130
#define DMAP_T2D                135
#define DMAP_U2E                140
#define DMAP_T2E                145
#define DMAP_S28                220
#define DMAP_D28                230
#define DMAP_E28                240
#define DMAP_82S                310
#define DMAP_82D                320
#define DMAP_82E                330
#define DMAP_U28                410
#define DMAP_82U                420
#define DMAP_T28                425
#define DMAP_U2U                510


typedef struct __dmap_rec       *dmap_t;

struct __dmap_rec
{
  uint32_t              codingSchema;
  unsigned char *       dmapS2S;        /* SBCS -> SBCS                         */
  /* The following conversion needs be followed by conversion from UCS-2/UTF-16 to UTF-8   */
  UniChar *     dmapD12U;       /* DBCS(non-EUC) -> UCS-2/UTF-16        */
  UniChar *     dmapD22U;       /* DBCS(non-EUC) -> UCS-2/UTF-16        */
  UniChar *     dmapE02U;       /* EUC/SS0 -> UCS-2/UTF-16              */
  UniChar *     dmapE12U;       /* EUC/SS1 -> UCS-2/UTF-16              */
  UniChar *     dmapE22U;       /* EUC/0x8E + SS2 -> UCS-2/UTF-16       */
  UniChar *     dmapE32U;       /* EUC/0x8F + SS3 -> UCS-2/UTF-16       */
  uchar *       dmapU2D;        /* UCS-2 -> DBCS                        */
  uchar *       dmapU2S;        /* UCS-2 -> EUC SS0                     */
  uchar *       dmapU2M2;       /* UCS-2 -> EUC SS1                     */
  uchar *       dmapU2M3;       /* UCS-2 -> EUC SS2/SS3                 */
  /* All of these pointers/tables are not used at the same time.        
   * You may be able save some space if you consolidate them.
   */
  uchar *       dmapS28;        /* SBCS -> UTF-8                        */
  uchar *       dmapD28;        /* DBCS -> UTF-8                        */
};

typedef	struct __myconv_rec	*myconv_t;
struct __myconv_rec
{
  uint32_t      converterType;
  uint32_t      index;          /* for close */
  union {
    iconv_t     cnv_iconv;
    dmap_t      cnv_dmap;
  };
  int32_t       allocatedSize;
  int32_t       fromCcsid;
  int32_t       toCcsid;
  UniChar       subD;           /* DBCS substitution char                       */
  char          subS;           /* SBCS substitution char                       */
  UniChar       srcSubD;        /* DBCS substitution char of src codepage       */
  char          srcSubS;        /* SBCS substitution char of src codepage       */
  char          from    [41+1]; /* codepage name is up to 41 bytes              */
  char          to      [41+1]; /* codepage name is up to 41 bytes              */
#ifdef __64BIT__
  char          reserved[10];   /* align 128 */
#else
  char          reserved[14];   /* align 128 */
#endif
};


EXTERN int32_t myconvDebug;



EXTERN  int     myconvGetES(CCSID);
EXTERN  int     myconvIsEBCDIC(const char *);
EXTERN  int     myconvIsASCII(const char *);
EXTERN  int     myconvIsUnicode(const char *);   /* UTF-8, UTF-16, or UCS-2 */
EXTERN  int     myconvIsUnicode2(const char *);  /* 2 byte Unicode */
EXTERN  int     myconvIsUCS2(const char *);
EXTERN  int     myconvIsUTF16(const char *);
EXTERN  int     myconvIsUTF8(const char *);
EXTERN  int     myconvIsEUC(const char *);
EXTERN  int     myconvIsISO(const char *);
EXTERN  int     myconvIsSBCS(const char *);
EXTERN  int     myconvIsDBCS(const char *);
EXTERN  char    myconvGetSubS(const char *);
EXTERN  UniChar myconvGetSubD(const char *);


EXTERN	myconv_t        myconv_open(const char*, const char*, int32_t);
EXTERN	int	        myconv_close(myconv_t);

INTERN  size_t	        myconv_iconv(myconv_t   cd ,
                                     char**     inBuf,
                                     size_t*    inBytesLeft,
                                     char**     outBuf,
                                     size_t*    outBytesLeft,
                                     size_t*    numSub)
{
  return iconv(cd->cnv_iconv, inBuf, inBytesLeft, outBuf, outBytesLeft);
}

INTERN  size_t	        myconv_dmap(myconv_t    cd,
                                    char**      inBuf,
                                    size_t*     inBytesLeft,
                                    char**      outBuf,
                                    size_t*     outBytesLeft,
                                    size_t*     numSub)
{
  if (cd->cnv_dmap->codingSchema == DMAP_S2S) {
    register unsigned char *   dmapS2S=cd->cnv_dmap->dmapS2S;
    register int        inLen=*inBytesLeft;
    register char *     pOut=*outBuf;
    register char *     pIn=*inBuf;
    register char *     pLastOutBuf = *outBuf + *outBytesLeft - 1;
    register char       subS=cd->subS;
    register size_t     numS=0;
    while (0 < inLen) {
      if (pLastOutBuf < pOut)
        break;
      if (*pIn == 0x00) {
        *pOut=0x00;
      } else {
        *pOut=dmapS2S[*pIn];
        if (*pOut == 0x00) {
          errno=EILSEQ;  /* 116 */
          *outBytesLeft-=(*inBytesLeft-inLen);
          *inBytesLeft=inLen;
          *outBuf=pOut;
          *inBuf=pIn;
          *numSub+=numS;
          return -1;
        }
        if (*pOut == subS) {
          if ((*pOut=dmapS2S[*pIn]) == subS) {
            if (*pIn != cd->srcSubS)
              ++numS;
          }
        }
      }
      ++pIn;
      --inLen;
      ++pOut;
    }
    *outBytesLeft-=(*inBytesLeft-inLen);
    *inBytesLeft=inLen;
    *outBuf=pOut;
    *inBuf=pIn;
    *numSub+=numS;
    return 0;

  } else if (cd->cnv_dmap->codingSchema == DMAP_E2U) {
    /* use uchar * instead of UniChar to avoid memcpy */
    register uchar *    dmapE02U=(uchar *) (cd->cnv_dmap->dmapE02U);
    register uchar *    dmapE12U=(uchar *) (cd->cnv_dmap->dmapE12U);
    register uchar *    dmapE22U=(uchar *) (cd->cnv_dmap->dmapE22U);
    register uchar *    dmapE32U=(uchar *) (cd->cnv_dmap->dmapE32U);
    register int        inLen=*inBytesLeft;
    register char *     pOut=*outBuf;
    register char *     pIn=*inBuf;
    register int        offset;
    register char *     pLastOutBuf = *outBuf + *outBytesLeft - 1;
    register size_t     numS=0;
    while (0 < inLen) {
      if (pLastOutBuf < pOut)
        break;
      if (*pIn == 0x00) {
        *pOut=0x00;
        ++pOut;
        *pOut=0x00;
        ++pOut;
        ++pIn;
        --inLen;
      } else {
        if (*pIn == 0x8E) {      /* SS2 */
          if (inLen < 2) {
            if (cd->fromCcsid == 33722 ||       /* IBM-eucJP */
                cd->fromCcsid == 964)           /* IBM-eucTW */
              errno=EINVAL;  /* 22 */
            else
              errno=EILSEQ;  /* 116 */
            *outBytesLeft-=(pOut-*outBuf);
            *inBytesLeft=inLen;
            *outBuf=pOut;
            *inBuf=pIn;
            return -1;
          }
          ++pIn;
          if (*pIn < 0xA0) {
            if (cd->fromCcsid == 964)           /* IBM-eucTW */
              errno=EINVAL;  /* 22 */
            else
              errno=EILSEQ;  /* 116 */
            *outBytesLeft-=(pOut-*outBuf);
            *inBytesLeft=inLen;
            *outBuf=pOut;
            *inBuf=pIn-1;
            return -1;
          }
          offset=(*pIn - 0xA0);
          offset<<=1;
          if (dmapE22U[offset]   == 0x00 &&
              dmapE22U[offset+1] == 0x00) {     /* 2 bytes */
            if (inLen < 3) {
              if (cd->fromCcsid == 964)           /* IBM-eucTW */
                errno=EINVAL;  /* 22 */
              else
                errno=EILSEQ;  /* 116 */
              *outBytesLeft-=(pOut-*outBuf);
              *inBytesLeft=inLen;
              *outBuf=pOut;
              *inBuf=pIn-1;
              return -1;
            }
            offset=(*pIn - 0xA0) * 0x60 + 0x60;
            ++pIn;
            if (*pIn < 0xA0) {
              if (cd->fromCcsid == 964)           /* IBM-eucTW */
                errno=EINVAL;  /* 22 */
              else
                errno=EILSEQ;  /* 116 */
              *outBytesLeft-=(pOut-*outBuf);
              *inBytesLeft=inLen;
              *outBuf=pOut;
              *inBuf=pIn-2;
              return -1;
            }
            offset+=(*pIn - 0xA0);
            offset<<=1;
            if (dmapE22U[offset] == 0x00 &&
                dmapE22U[offset+1] == 0x00) {
              if (cd->fromCcsid == 964)           /* IBM-eucTW */
                errno=EINVAL;  /* 22 */
              else
                errno=EILSEQ;  /* 116 */
              *outBytesLeft-=(pOut-*outBuf);
              *inBytesLeft=inLen;
              *outBuf=pOut;
              *inBuf=pIn-2;
              return -1;
            }
            *pOut=dmapE22U[offset];
            ++pOut;
            *pOut=dmapE22U[offset+1];
            ++pOut;
            if (dmapE22U[offset] == 0xFF &&
                dmapE22U[offset+1] == 0xFD) {
              if (pIn[-2] * 0x100 + pIn[-1] != cd->srcSubD)
                ++numS;
            }
            ++pIn;
            inLen-=3;
          } else {      /* 1 bytes */
            *pOut=dmapE22U[offset];
            ++pOut;
            *pOut=dmapE22U[offset+1];
            ++pOut;
            ++pIn;
            inLen-=2;
          }
        } else if (*pIn == 0x8F) {     /* SS3 */
          if (inLen < 2) {
            if (cd->fromCcsid == 33722)   /* IBM-eucJP */
              errno=EINVAL;  /* 22 */
            else
              errno=EILSEQ;  /* 116 */
            *outBytesLeft-=(pOut-*outBuf);
            *inBytesLeft=inLen;
            *outBuf=pOut;
            *inBuf=pIn;
            return -1;
          }
          ++pIn;
          if (*pIn < 0xA0) {
            if (cd->fromCcsid == 970 ||         /* IBM-eucKR */
                cd->fromCcsid == 964 ||         /* IBM-eucTW */
                cd->fromCcsid == 1383 ||        /* IBM-eucCN */
                (cd->fromCcsid == 33722 && 3 <= inLen)) /* IBM-eucJP */
              errno=EILSEQ;  /* 116 */
            else
              errno=EINVAL;  /* 22 */
            *outBytesLeft-=(pOut-*outBuf);
            *inBytesLeft=inLen;
            *outBuf=pOut;
            *inBuf=pIn-1;
            return -1;
          }
          offset=(*pIn - 0xA0);
          offset<<=1;
          if (dmapE32U[offset]   == 0x00 &&
              dmapE32U[offset+1] == 0x00) { /* 0x8F + 2 bytes */
            if (inLen < 3) {
              if (cd->fromCcsid == 33722)
                errno=EINVAL;  /* 22 */
              else
                errno=EILSEQ;  /* 116 */
              *outBytesLeft-=(pOut-*outBuf);
              *inBytesLeft=inLen;
              *outBuf=pOut;
              *inBuf=pIn-1;
              return -1;
            }
            offset=(*pIn - 0xA0) * 0x60 + 0x60;
            ++pIn;
            if (*pIn < 0xA0) {
              errno=EILSEQ;  /* 116 */
              *outBytesLeft-=(pOut-*outBuf);
              *inBytesLeft=inLen;
              *outBuf=pOut;
              *inBuf=pIn-2;
              return -1;
            }
            offset+=(*pIn - 0xA0);
            offset<<=1;
            if (dmapE32U[offset] == 0x00 &&
                dmapE32U[offset+1] == 0x00) {
              errno=EILSEQ;  /* 116 */
              *outBytesLeft-=(pOut-*outBuf);
              *inBytesLeft=inLen;
              *outBuf=pOut;
              *inBuf=pIn-2;
              return -1;
            }
            *pOut=dmapE32U[offset];
            ++pOut;
            *pOut=dmapE32U[offset+1];
            ++pOut;
            if (dmapE32U[offset] == 0xFF &&
                dmapE32U[offset+1] == 0xFD) {
              if (pIn[-2] * 0x100 + pIn[-1] != cd->srcSubD)
                ++numS;
            }
            ++pIn;
            inLen-=3;
          } else {      /* 0x8F + 1 bytes */
            *pOut=dmapE32U[offset];
            ++pOut;
            *pOut=dmapE32U[offset+1];
            ++pOut;
            ++pIn;
            inLen-=2;
          }

        } else {
          offset=*pIn;
          offset<<=1;
          if (dmapE02U[offset]   == 0x00 &&
              dmapE02U[offset+1] == 0x00) {         /* SS1 */
            if (inLen < 2) {
              if ((cd->fromCcsid == 33722 && (*pIn == 0xA0 || (0xA9 <= *pIn && *pIn <= 0xAF) || *pIn == 0xFF)) ||
                  (cd->fromCcsid == 970 && (*pIn == 0xA0 || *pIn == 0xAD || *pIn == 0xAE || *pIn == 0xAF || *pIn == 0xFF)) ||
                  (cd->fromCcsid == 964 && (*pIn == 0xA0 || (0xAA <= *pIn && *pIn <= 0xC1) || *pIn == 0xC3 || *pIn == 0xFE || *pIn == 0xFF)) ||
                  (cd->fromCcsid == 1383 && (*pIn == 0xA0 || *pIn == 0xFF)))
                errno=EILSEQ;  /* 116 */
              else
                errno=EINVAL;  /* 22 */
              *outBytesLeft-=(pOut-*outBuf);
              *inBytesLeft=inLen;
              *outBuf=pOut;
              *inBuf=pIn;
              return -1;
            }
            if (*pIn < 0xA0) {
              errno=EILSEQ;  /* 116 */
              *outBytesLeft-=(pOut-*outBuf);
              *inBytesLeft=inLen;
              *outBuf=pOut;
              *inBuf=pIn;
              return -1;
            }
            offset=(*pIn - 0xA0) * 0x60;
            ++pIn;
            if (*pIn < 0xA0) {
              errno=EILSEQ;  /* 116 */
              *outBytesLeft-=(pOut-*outBuf);
              *inBytesLeft=inLen;
              *outBuf=pOut;
              *inBuf=pIn-1;
              return -1;
            }
            offset+=(*pIn - 0xA0);
            offset<<=1;
            if (dmapE12U[offset] == 0x00 &&
                dmapE12U[offset+1] == 0x00) {   /* undefined mapping */
              errno=EILSEQ;  /* 116 */
              *outBytesLeft-=(pOut-*outBuf);
              *inBytesLeft=inLen;
              *outBuf=pOut;
              *inBuf=pIn-1;
              return -1;
            }
            *pOut=dmapE12U[offset];
            ++pOut;
            *pOut=dmapE12U[offset+1];
            ++pOut;
            if (dmapE12U[offset] == 0xFF &&
                dmapE12U[offset+1] == 0xFD) {
              if (pIn[-1] * 0x100 + pIn[0] != cd->srcSubD)
                ++numS;
            }
            ++pIn;
            inLen-=2;
          } else {
            *pOut=dmapE02U[offset];
            ++pOut;
            *pOut=dmapE02U[offset+1];
            ++pOut;
            if (dmapE02U[offset] == 0x00 &&
                dmapE02U[offset+1] == 0x1A) {
              if (*pIn != cd->srcSubS)
                ++numS;
            }
            ++pIn;
            --inLen;
          }
        }
      }
    }
    *outBytesLeft-=(pOut-*outBuf);
    *inBytesLeft=inLen;
    *outBuf=pOut;
    *inBuf=pIn;
    *numSub+=numS;
    return 0;


  } else if (cd->cnv_dmap->codingSchema == DMAP_E28) {
    /* use uchar * instead of UniChar to avoid memcpy */
    register uchar *    dmapE02U=(uchar *) (cd->cnv_dmap->dmapE02U);
    register uchar *    dmapE12U=(uchar *) (cd->cnv_dmap->dmapE12U);
    register uchar *    dmapE22U=(uchar *) (cd->cnv_dmap->dmapE22U);
    register uchar *    dmapE32U=(uchar *) (cd->cnv_dmap->dmapE32U);
    register int        inLen=*inBytesLeft;
    register char *     pOut=*outBuf;
    register char *     pIn=*inBuf;
    register int        offset;
    register char *     pLastOutBuf = *outBuf + *outBytesLeft - 1;
    register size_t     numS=0;
    register UniChar    in;     /* copy part of U28 */
    register UniChar    ucs2;
    while (0 < inLen) {
      if (pLastOutBuf < pOut)
        break;
      if (*pIn == 0x00) {
        *pOut=0x00;
        ++pOut;
        ++pIn;
        --inLen;
      } else {
        if (*pIn == 0x8E) {      /* SS2 */
          if (inLen < 2) {
            if (cd->fromCcsid == 33722 ||       /* IBM-eucJP */
                cd->fromCcsid == 964)           /* IBM-eucTW */
              errno=EINVAL;  /* 22 */
            else
              errno=EILSEQ;  /* 116 */
            *outBytesLeft-=(pOut-*outBuf);
            *inBytesLeft=inLen;
            *outBuf=pOut;
            *inBuf=pIn;
            return -1;
          }
          ++pIn;
          if (*pIn < 0xA0) {
            if (cd->fromCcsid == 964)           /* IBM-eucTW */
              errno=EINVAL;  /* 22 */
            else
              errno=EILSEQ;  /* 116 */
            *outBytesLeft-=(pOut-*outBuf);
            *inBytesLeft=inLen;
            *outBuf=pOut;
            *inBuf=pIn-1;
            return -1;
          }
          offset=(*pIn - 0xA0);
          offset<<=1;
          if (dmapE22U[offset]   == 0x00 &&
              dmapE22U[offset+1] == 0x00) {     /* 2 bytes */
            if (inLen < 3) {
              if (cd->fromCcsid == 964)           /* IBM-eucTW */
                errno=EINVAL;  /* 22 */
              else
                errno=EILSEQ;  /* 116 */
              *outBytesLeft-=(pOut-*outBuf);
              *inBytesLeft=inLen;
              *outBuf=pOut;
              *inBuf=pIn-1;
              return -1;
            }
            offset=(*pIn - 0xA0) * 0x60 + 0x60;
            ++pIn;
            if (*pIn < 0xA0) {
              if (cd->fromCcsid == 964)           /* IBM-eucTW */
                errno=EINVAL;  /* 22 */
              else
                errno=EILSEQ;  /* 116 */
              *outBytesLeft-=(pOut-*outBuf);
              *inBytesLeft=inLen;
              *outBuf=pOut;
              *inBuf=pIn-2;
              return -1;
            }
            offset+=(*pIn - 0xA0);
            offset<<=1;
            if (dmapE22U[offset] == 0x00 &&
                dmapE22U[offset+1] == 0x00) {
              if (cd->fromCcsid == 964)           /* IBM-eucTW */
                errno=EINVAL;  /* 22 */
              else
                errno=EILSEQ;  /* 116 */
              *outBytesLeft-=(pOut-*outBuf);
              *inBytesLeft=inLen;
              *outBuf=pOut;
              *inBuf=pIn-2;
              return -1;
            }
            in=dmapE22U[offset];
            in<<=8;
            in+=dmapE22U[offset+1];
            if (dmapE22U[offset] == 0xFF &&
                dmapE22U[offset+1] == 0xFD) {
              if (pIn[-2] * 0x100 + pIn[-1] != cd->srcSubD)
                ++numS;
            }
            ++pIn;
            inLen-=3;
          } else {      /* 1 bytes */
            in=dmapE22U[offset];
            in<<=8;
            in+=dmapE22U[offset+1];
            ++pIn;
            inLen-=2;
          }
        } else if (*pIn == 0x8F) {     /* SS3 */
          if (inLen < 2) {
            if (cd->fromCcsid == 33722)   /* IBM-eucJP */
              errno=EINVAL;  /* 22 */
            else
              errno=EILSEQ;  /* 116 */
            *outBytesLeft-=(pOut-*outBuf);
            *inBytesLeft=inLen;
            *outBuf=pOut;
            *inBuf=pIn;
            return -1;
          }
          ++pIn;
          if (*pIn < 0xA0) {
            if (cd->fromCcsid == 970 ||         /* IBM-eucKR */
                cd->fromCcsid == 964 ||         /* IBM-eucTW */
                cd->fromCcsid == 1383 ||        /* IBM-eucCN */
                (cd->fromCcsid == 33722 && 3 <= inLen)) /* IBM-eucJP */
              errno=EILSEQ;  /* 116 */
            else
              errno=EINVAL;  /* 22 */
            *outBytesLeft-=(pOut-*outBuf);
            *inBytesLeft=inLen;
            *outBuf=pOut;
            *inBuf=pIn-1;
            return -1;
          }
          offset=(*pIn - 0xA0);
          offset<<=1;
          if (dmapE32U[offset]   == 0x00 &&
              dmapE32U[offset+1] == 0x00) { /* 0x8F + 2 bytes */
            if (inLen < 3) {
              if (cd->fromCcsid == 33722)
                errno=EINVAL;  /* 22 */
              else
                errno=EILSEQ;  /* 116 */
              *outBytesLeft-=(pOut-*outBuf);
              *inBytesLeft=inLen;
              *outBuf=pOut;
              *inBuf=pIn-1;
              return -1;
            }
            offset=(*pIn - 0xA0) * 0x60 + 0x60;
            ++pIn;
            if (*pIn < 0xA0) {
              errno=EILSEQ;  /* 116 */
              *outBytesLeft-=(pOut-*outBuf);
              *inBytesLeft=inLen;
              *outBuf=pOut;
              *inBuf=pIn-2;
              return -1;
            }
            offset+=(*pIn - 0xA0);
            offset<<=1;
            if (dmapE32U[offset] == 0x00 &&
                dmapE32U[offset+1] == 0x00) {
              errno=EILSEQ;  /* 116 */
              *outBytesLeft-=(pOut-*outBuf);
              *inBytesLeft=inLen;
              *outBuf=pOut;
              *inBuf=pIn-2;
              return -1;
            }
            in=dmapE32U[offset];
            in<<=8;
            in+=dmapE32U[offset+1];
            if (dmapE32U[offset] == 0xFF &&
                dmapE32U[offset+1] == 0xFD) {
              if (pIn[-2] * 0x100 + pIn[-1] != cd->srcSubD)
                ++numS;
            }
            ++pIn;
            inLen-=3;
          } else {      /* 0x8F + 1 bytes */
            in=dmapE32U[offset];
            in<<=8;
            in+=dmapE32U[offset+1];
            ++pIn;
            inLen-=2;
          }

        } else {
          offset=*pIn;
          offset<<=1;
          if (dmapE02U[offset]   == 0x00 &&
              dmapE02U[offset+1] == 0x00) {         /* SS1 */
            if (inLen < 2) {
              if ((cd->fromCcsid == 33722 && (*pIn == 0xA0 || (0xA9 <= *pIn && *pIn <= 0xAF) || *pIn == 0xFF)) ||
                  (cd->fromCcsid == 970 && (*pIn == 0xA0 || *pIn == 0xAD || *pIn == 0xAE || *pIn == 0xAF || *pIn == 0xFF)) ||
                  (cd->fromCcsid == 964 && (*pIn == 0xA0 || (0xAA <= *pIn && *pIn <= 0xC1) || *pIn == 0xC3 || *pIn == 0xFE || *pIn == 0xFF)) ||
                  (cd->fromCcsid == 1383 && (*pIn == 0xA0 || *pIn == 0xFF)))
                errno=EILSEQ;  /* 116 */
              else
                errno=EINVAL;  /* 22 */
              *outBytesLeft-=(pOut-*outBuf);
              *inBytesLeft=inLen;
              *outBuf=pOut;
              *inBuf=pIn;
              return -1;
            }
            if (*pIn < 0xA0) {
              errno=EILSEQ;  /* 116 */
              *outBytesLeft-=(pOut-*outBuf);
              *inBytesLeft=inLen;
              *outBuf=pOut;
              *inBuf=pIn;
              return -1;
            }
            offset=(*pIn - 0xA0) * 0x60;
            ++pIn;
            if (*pIn < 0xA0) {
              errno=EILSEQ;  /* 116 */
              *outBytesLeft-=(pOut-*outBuf);
              *inBytesLeft=inLen;
              *outBuf=pOut;
              *inBuf=pIn-1;
              return -1;
            }
            offset+=(*pIn - 0xA0);
            offset<<=1;
            if (dmapE12U[offset] == 0x00 &&
                dmapE12U[offset+1] == 0x00) {   /* undefined mapping */
              errno=EILSEQ;  /* 116 */
              *outBytesLeft-=(pOut-*outBuf);
              *inBytesLeft=inLen;
              *outBuf=pOut;
              *inBuf=pIn-1;
              return -1;
            }
            in=dmapE12U[offset];
            in<<=8;
            in+=dmapE12U[offset+1];
            if (dmapE12U[offset] == 0xFF &&
                dmapE12U[offset+1] == 0xFD) {
              if (pIn[-1] * 0x100 + pIn[0] != cd->srcSubD)
                ++numS;
            }
            ++pIn;
            inLen-=2;
          } else {
            in=dmapE02U[offset];
            in<<=8;
            in+=dmapE02U[offset+1];
            if (dmapE02U[offset] == 0x00 &&
                dmapE02U[offset+1] == 0x1A) {
              if (*pIn != cd->srcSubS)
                ++numS;
            }
            ++pIn;
            --inLen;
          }
        }
        ucs2=in;
        if ((in & 0xFF80) == 0x0000) {     /* U28: in & 0b1111111110000000 == 0x0000 */
          *pOut=in;
          ++pOut;
        } else if ((in & 0xF800) == 0x0000) {     /* in & 0b1111100000000000 == 0x0000 */
          register uchar        byte;
          in>>=6;
          in&=0x001F;   /* 0b0000000000011111 */
          in|=0x00C0;   /* 0b0000000011000000 */
          *pOut=in;
          ++pOut;
          byte=ucs2;    /* dmapD12U[offset+1]; */
          byte&=0x3F;   /* 0b00111111; */
          byte|=0x80;   /* 0b10000000; */
          *pOut=byte;
          ++pOut;
        } else if ((in & 0xFC00) == 0xD800) {
          *pOut=0xEF;
          ++pOut;
          *pOut=0xBF;
          ++pOut;
          *pOut=0xBD;
          ++pOut;
        } else {
          register uchar        byte;
          register uchar        work;
          byte=(ucs2>>8);    /* dmapD12U[offset]; */
          byte>>=4;
          byte|=0xE0;   /* 0b11100000; */
          *pOut=byte;
          ++pOut;

          byte=(ucs2>>8);    /* dmapD12U[offset]; */
          byte<<=2;
          work=ucs2;    /* dmapD12U[offset+1]; */
          work>>=6;
          byte|=work;
          byte&=0x3F;   /* 0b00111111; */
          byte|=0x80;   /* 0b10000000; */
          *pOut=byte;
          ++pOut;

          byte=ucs2;    /* dmapD12U[offset+1]; */
          byte&=0x3F;   /* 0b00111111; */
          byte|=0x80;   /* 0b10000000; */
          *pOut=byte;
          ++pOut;
        }
        /* end of U28 */
      }
    }
    *outBytesLeft-=(pOut-*outBuf);
    *inBytesLeft=inLen;
    *outBuf=pOut;
    *inBuf=pIn;
    *numSub+=numS;
    return 0;

  } else if (cd->cnv_dmap->codingSchema == DMAP_U2E) {
    register uchar *    dmapU2S=cd->cnv_dmap->dmapU2S;
    register uchar *    dmapU2M2=cd->cnv_dmap->dmapU2M2 - 0x80 * 2;
    register uchar *    dmapU2M3=cd->cnv_dmap->dmapU2M3 - 0x80 * 3;
    register int        inLen=*inBytesLeft;
    register char *     pOut=*outBuf;
    register char *     pIn=*inBuf;
    register char *     pLastOutBuf = *outBuf + *outBytesLeft - 1;
    register char       subS=cd->subS;
    register char *     pSubD=(char *) &(cd->subD);
    register size_t     numS=0;
    register size_t     rc=0;
    while (0 < inLen) {
      register uint32_t       in;
      if (inLen == 1) {
        errno=EINVAL;  /* 22 */
        *outBytesLeft-=(pOut-*outBuf);
        *inBytesLeft=inLen;
        *outBuf=pOut;
        *inBuf=pIn;
        return -1;
      }
      if (pLastOutBuf < pOut)
        break;
      in=pIn[0];
      in<<=8;
      in+=pIn[1];
      if (in == 0x0000) {
        *pOut=0x00;
        ++pOut;
      } else if (in < 0x100 && dmapU2S[in] != 0x0000) {
        if ((*pOut=dmapU2S[in]) == subS) {
          if (in != cd->srcSubS)
            ++numS;
        }
        ++pOut;
      } else {
        in<<=1;
        if (dmapU2M2[in] == 0x00) {     /* not found in dmapU2M2 */
          in*=1.5;
          if (dmapU2M3[in] == 0x00) {   /* not found in dmapU2M3*/
            *pOut=pSubD[0];
            ++pOut;
            *pOut=pSubD[1];
            ++pOut;
            ++numS;
            ++rc;
          } else {
            *pOut=dmapU2M3[in];
            ++pOut;
            *pOut=dmapU2M3[1+in];
            ++pOut;
            *pOut=dmapU2M3[2+in];
            ++pOut;
          }
        } else {
          *pOut=dmapU2M2[in];
          ++pOut;
          if (dmapU2M2[1+in] == 0x00) {
            if (*pOut == subS) {
              in>>=1;
              if (in != cd->srcSubS)
                ++numS;
            }
          } else {
            *pOut=dmapU2M2[1+in];
            ++pOut;
            if (memcmp(pOut-2, pSubD, 2) == 0) {
              in>>=1;
              if (in != cd->srcSubD) {
                ++numS;
                ++rc;
              }
            }
          }
        }
      }
      pIn+=2;
      inLen-=2;
    }
    *outBytesLeft-=(pOut-*outBuf);
    *inBytesLeft=inLen;
    *outBuf=pOut;
    *inBuf=pIn;
    *numSub+=numS;
    return rc;        /* compatibility to iconv() */

  } else if (cd->cnv_dmap->codingSchema == DMAP_T2E) {
    register uchar *    dmapU2S=cd->cnv_dmap->dmapU2S;
    register uchar *    dmapU2M2=cd->cnv_dmap->dmapU2M2 - 0x80 * 2;
    register uchar *    dmapU2M3=cd->cnv_dmap->dmapU2M3 - 0x80 * 3;
    register int        inLen=*inBytesLeft;
    register char *     pOut=*outBuf;
    register char *     pIn=*inBuf;
    register char *     pLastOutBuf = *outBuf + *outBytesLeft - 1;
    register char       subS=cd->subS;
    register char *     pSubD=(char *) &(cd->subD);
    register size_t     numS=0;
    register size_t     rc=0;
    while (0 < inLen) {
      register uint32_t       in;
      if (inLen == 1) {
        errno=EINVAL;  /* 22 */
        *outBytesLeft-=(pOut-*outBuf);
        *inBytesLeft=inLen-1;
        *outBuf=pOut;
        *inBuf=pIn;
        ++numS;
        *numSub+=numS;
        return 0;
      }
      if (pLastOutBuf < pOut)
        break;
      in=pIn[0];
      in<<=8;
      in+=pIn[1];
      if (in == 0x0000) {
        *pOut=0x00;
        ++pOut;
      } else if (0xD800 <= in && in <= 0xDBFF) { /* first byte of surrogate */
        errno=EINVAL;   /* 22 */
        *inBytesLeft=inLen-2;
        *outBytesLeft-=(pOut-*outBuf);
        *outBuf=pOut;
        *inBuf=pIn+2;
        ++numS;
        *numSub+=numS;
        return -1;
        
      } else if (0xDC00 <= in && in <= 0xDFFF) { /* second byte of surrogate */
        errno=EINVAL;   /* 22 */
        *inBytesLeft=inLen-1;
        *outBytesLeft-=(pOut-*outBuf);
        *outBuf=pOut;
        *inBuf=pIn;
        ++numS;
        *numSub+=numS;
        return -1;
        
      } else if (in < 0x100 && dmapU2S[in] != 0x0000) {
        if ((*pOut=dmapU2S[in]) == subS) {
          if (in != cd->srcSubS)
            ++numS;
        }
        ++pOut;
      } else {
        in<<=1;
        if (dmapU2M2[in] == 0x00) {     /* not found in dmapU2M2 */
          in*=1.5;
          if (dmapU2M3[in] == 0x00) {   /* not found in dmapU2M3*/
            *pOut=pSubD[0];
            ++pOut;
            *pOut=pSubD[1];
            ++pOut;
            ++numS;
            ++rc;
          } else {
            *pOut=dmapU2M3[in];
            ++pOut;
            *pOut=dmapU2M3[1+in];
            ++pOut;
            *pOut=dmapU2M3[2+in];
            ++pOut;
          }
        } else {
          *pOut=dmapU2M2[in];
          ++pOut;
          if (dmapU2M2[1+in] == 0x00) {
            if (*pOut == subS) {
              in>>=1;
              if (in != cd->srcSubS)
                ++numS;
            }
          } else {
            *pOut=dmapU2M2[1+in];
            ++pOut;
            if (memcmp(pOut-2, pSubD, 2) == 0) {
              in>>=1;
              if (in != cd->srcSubD) {
                ++numS;
                ++rc;
              }
            }
          }
        }
      }
      pIn+=2;
      inLen-=2;
    }
    *outBytesLeft-=(pOut-*outBuf);
    *inBytesLeft=inLen;
    *outBuf=pOut;
    *inBuf=pIn;
    *numSub+=numS;
    return 0;

  } else if (cd->cnv_dmap->codingSchema == DMAP_82E) {
    register uchar *    dmapU2S=cd->cnv_dmap->dmapU2S;
    register uchar *    dmapU2M2=cd->cnv_dmap->dmapU2M2 - 0x80 * 2;
    register uchar *    dmapU2M3=cd->cnv_dmap->dmapU2M3 - 0x80 * 3;
    register int        inLen=*inBytesLeft;
    register char *     pOut=*outBuf;
    register char *     pIn=*inBuf;
    register char *     pLastOutBuf = *outBuf + *outBytesLeft - 1;
    register char       subS=cd->subS;
    register char *     pSubD=(char *) &(cd->subD);
    register size_t     numS=0;
    register size_t     rc=0;
    while (0 < inLen) {
      register uint32_t in;
      uint32_t          in2;
      if (pLastOutBuf < pOut)
        break;
      /* convert from UTF-8 to UCS-2 */
      if (*pIn == 0x00) {
        in=0x0000;
        ++pIn;
        --inLen;
      } else {                          /* 82U: */
        register uchar byte1=*pIn;
        if ((byte1 & 0x80) == 0x00) {         /* if (byte1 & 0b10000000 == 0b00000000) { */
          /* 1 bytes sequence:  0xxxxxxx => 00000000 0xxxxxxx*/
          in=byte1;
          ++pIn;
          --inLen;
        } else if ((byte1 & 0xE0) == 0xC0) {  /* (byte1 & 0b11100000 == 0b11000000) { */
          if (inLen < 2) {
            errno=EINVAL;  /* 22 */
            *outBytesLeft-=(pOut-*outBuf);
            *inBytesLeft=inLen;
            *outBuf=pOut;
            *inBuf=pIn;
            *numSub+=numS;
            return -1;
          }
          if (byte1 == 0xC0 || byte1 == 0xC1) {  /* invalid sequence */
            errno=EILSEQ;  /* 116 */
            *outBytesLeft-=(pOut-*outBuf);
            *inBytesLeft=inLen;
            *outBuf=pOut;
            *inBuf=pIn;
            *numSub+=numS;
            return -1;
          }
          /* 2 bytes sequence: 
             110yyyyy 10xxxxxx => 00000yyy yyxxxxxx */
          register uchar byte2;
          ++pIn;
          byte2=*pIn;
          if ((byte2 & 0xC0) == 0x80) {       /* byte2 & 0b11000000 == 0b10000000) { */
            register uchar work=byte1;
            work<<=6;
            byte2&=0x3F;      /* 0b00111111; */
            byte2|=work;
              
            byte1&=0x1F;      /* 0b00011111; */
            byte1>>=2;
            in=byte1;
            in<<=8;
            in+=byte2;
            inLen-=2;
            ++pIn;
          } else { /* invalid sequence */
            errno=EILSEQ;  /* 116 */
            *outBytesLeft-=(pOut-*outBuf);
            *inBytesLeft=inLen;
            *outBuf=pOut;
            *inBuf=pIn-1;
            *numSub+=numS;
            return -1;
          }
        } else if ((byte1 & 0xF0) == 0xE0) {    /* byte1 & 0b11110000 == 0b11100000 */
          /* 3 bytes sequence: 
             1110zzzz 10yyyyyy 10xxxxxx => zzzzyyyy yyxxxxxx */
          register uchar byte2;
          register uchar byte3;
          if (inLen < 3) {
            if (inLen == 2 && (pIn[1]  & 0xC0) != 0x80)
              errno=EILSEQ;  /* 116 */
            else
              errno=EINVAL;  /* 22 */
            *outBytesLeft-=(pOut-*outBuf);
            *inBytesLeft=inLen;
            *outBuf=pOut;
            *inBuf=pIn;
            *numSub+=numS;
            return -1;
          }
          ++pIn;
          byte2=*pIn;
          ++pIn;
          byte3=*pIn;
          if ((byte2 & 0xC0) != 0x80 ||
              (byte3 & 0xC0) != 0x80 ||
              (byte1 == 0xE0 && byte2 < 0xA0)) { /* invalid sequence, only 0xA0-0xBF allowed after 0xE0 */
            errno=EILSEQ;  /* 116 */
            *outBytesLeft-=(pOut-*outBuf);
            *inBytesLeft=inLen;
            *outBuf=pOut;
            *inBuf=pIn-2;
            *numSub+=numS;
            return -1;
          }
          {
            register uchar work=byte2;
            work<<=6;
            byte3&=0x3F;      /* 0b00111111; */
            byte3|=work;
              
            byte2&=0x3F;      /* 0b00111111; */
            byte2>>=2;

            byte1<<=4;
            in=byte1 | byte2;;
            in<<=8;
            in+=byte3;
            inLen-=3;
            ++pIn;
          }
        } else if ((0xF0 <= byte1 && byte1 <= 0xF4)) {    /* (bytes1 & 11111000) == 0x1110000 */
          /* 4 bytes sequence
             11110uuu 10uuzzzz 10yyyyyy 10xxxxxx => 110110ww wwzzzzyy 110111yy yyxxxxxx
             where uuuuu = wwww + 1 */
          register uchar byte2;
          register uchar byte3;
          register uchar byte4;
          if (inLen < 4) {
            if ((inLen >= 2 && (pIn[1]  & 0xC0) != 0x80) ||
                (inLen >= 3 && (pIn[2]  & 0xC0) != 0x80) ||
                (cd->toCcsid == 13488) )
              errno=EILSEQ;  /* 116 */
            else
              errno=EINVAL;  /* 22 */
            *outBytesLeft-=(pOut-*outBuf);
            *inBytesLeft=inLen;
            *outBuf=pOut;
            *inBuf=pIn;
            *numSub+=numS;
            return -1;
          }
          ++pIn;
          byte2=*pIn;
          ++pIn;
          byte3=*pIn;
          ++pIn;
          byte4=*pIn;
          if ((byte2 & 0xC0) == 0x80 &&         /* byte2 & 0b11000000 == 0b10000000 */
              (byte3 & 0xC0) == 0x80 &&         /* byte3 & 0b11000000 == 0b10000000 */
              (byte4 & 0xC0) == 0x80) {         /* byte4 & 0b11000000 == 0b10000000 */
            register uchar work=byte2;
            if (byte1 == 0xF0 && byte2 < 0x90) {
              errno=EILSEQ;  /* 116 */
              *outBytesLeft-=(pOut-*outBuf);
              *inBytesLeft=inLen;
              *outBuf=pOut;
              *inBuf=pIn-3;
              *numSub+=numS;
              return -1;
              /* iconv() returns 0 for 0xF4908080 and convert to 0x00 
            } else if (byte1 == 0xF4 && byte2 > 0x8F) {
              errno=EINVAL;
              *outBytesLeft-=(pOut-*outBuf);
              *inBytesLeft=inLen;
              *outBuf=pOut;
              *inBuf=pIn-3;
              *numSub+=numS;
              return -1;
              */
            }

            work&=0x30;       /* 0b00110000; */
            work>>=4;
            byte1&=0x07;      /* 0b00000111; */
            byte1<<=2;
            byte1+=work;      /* uuuuu */
            --byte1;          /* wwww  */

            work=byte1 & 0x0F;
            work>>=2;
            work+=0xD8;       /* 0b11011011; */
            in=work;
            in<<=8;

            byte1<<=6;
            byte2<<=2;
            byte2&=0x3C;      /* 0b00111100; */
            work=byte3;
            work>>=4;
            work&=0x03;       /* 0b00000011; */
            work|=byte1;
            work|=byte2;
            in+=work;

            work=byte3;
            work>>=2;
            work&=0x03;       /* 0b00000011; */
            work|=0xDC;       /* 0b110111xx; */
            in2=work;
            in2<<=8;

            byte3<<=6;
            byte4&=0x3F;      /* 0b00111111; */
            byte4|=byte3;
            in2+=byte4;
            inLen-=4;
            ++pIn;
#ifdef match_with_GBK
            if ((0xD800 == in && in2 < 0xDC80) ||
                (0xD840 == in && in2 < 0xDC80) ||
                (0xD880 == in && in2 < 0xDC80) ||
                (0xD8C0 == in && in2 < 0xDC80) ||
                (0xD900 == in && in2 < 0xDC80) ||
                (0xD940 == in && in2 < 0xDC80) ||
                (0xD980 == in && in2 < 0xDC80) ||
                (0xD9C0 == in && in2 < 0xDC80) ||
                (0xDA00 == in && in2 < 0xDC80) ||
                (0xDA40 == in && in2 < 0xDC80) ||
                (0xDA80 == in && in2 < 0xDC80) ||
                (0xDAC0 == in && in2 < 0xDC80) ||
                (0xDB00 == in && in2 < 0xDC80) ||
                (0xDB40 == in && in2 < 0xDC80) ||
                (0xDB80 == in && in2 < 0xDC80) ||
                (0xDBC0 == in && in2 < 0xDC80)) {
#else
              if ((0xD800 <= in  && in  <= 0xDBFF) &&
                  (0xDC00 <= in2 && in2 <= 0xDFFF)) {
#endif
              *pOut=subS;
              ++pOut;
              ++numS;
              continue;
            }
          } else { /* invalid sequence */
            errno=EILSEQ;  /* 116 */
            *outBytesLeft-=(pOut-*outBuf);
            *inBytesLeft=inLen;
            *outBuf=pOut;
            *inBuf=pIn-3;
            *numSub+=numS;
            return -1;
          }
        } else if (0xF5 <= byte1 && byte1 <= 0xFF) { /* minic iconv() behavior */
          if (inLen < 4 ||
              (inLen >= 4 && byte1 == 0xF8 && pIn[1] < 0x90) ||
              pIn[1] < 0x80 || 0xBF < pIn[1] ||
              pIn[2] < 0x80 || 0xBF < pIn[2] ||
              pIn[3] < 0x80 || 0xBF < pIn[3] ) {
            if (inLen == 1)
              errno=EINVAL;  /* 22 */
            else if (inLen == 2 && (pIn[1]  & 0xC0) != 0x80)
              errno=EILSEQ;  /* 116 */
            else if (inLen == 3 && ((pIn[1]  & 0xC0) != 0x80 || (pIn[2] & 0xC0) != 0x80))
              errno=EILSEQ;  /* 116 */
            else if (inLen >= 4 && (byte1 == 0xF8 || (pIn[1]  & 0xC0) != 0x80 || (pIn[2] & 0xC0) != 0x80 || (pIn[3] & 0xC0) != 0x80))
              errno=EILSEQ;  /* 116 */
            else
              errno=EINVAL;  /* 22 */

            *outBytesLeft-=(pOut-*outBuf);
            *inBytesLeft=inLen;
            *outBuf=pOut;
            *inBuf=pIn;
            *numSub+=numS;
            return -1;
          } else if ((pIn[1] == 0x80 || pIn[1] == 0x90 || pIn[1] == 0xA0 || pIn[1] == 0xB0) && 
                     pIn[2] < 0x82) {
            *pOut=subS;   /* Though returns replacement character, which iconv() does not return. */
            ++pOut;
            ++numS;
            pIn+=4;
            inLen-=4;
            continue;
          } else {
            *pOut=pSubD[0];   /* Though returns replacement character, which iconv() does not return. */
            ++pOut;
            *pOut=pSubD[1];
            ++pOut;
            ++numS;
            pIn+=4;
            inLen-=4;
            continue;
            /* iconv() returns 0 with strange 1 byte converted values */
          }

        } else { /* invalid sequence */
          errno=EILSEQ;  /* 116 */
          *outBytesLeft-=(pOut-*outBuf);
          *inBytesLeft=inLen;
          *outBuf=pOut;
          *inBuf=pIn;
          *numSub+=numS;
          return -1;
        }
      }
      /* end of UTF-8 to UCS-2 */
      if (in == 0x0000) {
        *pOut=0x00;
        ++pOut;
      } else if (in < 0x100 && dmapU2S[in] != 0x0000) {
        if ((*pOut=dmapU2S[in]) == subS) {
          if (in != cd->srcSubS)
            ++numS;
        }
        ++pOut;
      } else {
        in<<=1;
        if (dmapU2M2[in] == 0x00) {     /* not found in dmapU2M2 */
          in*=1.5;
          if (dmapU2M3[in] == 0x00) {   /* not found in dmapU2M3*/
            *pOut=pSubD[0];
            ++pOut;
            *pOut=pSubD[1];
            ++pOut;
            ++numS;
            ++rc;
          } else {
            *pOut=dmapU2M3[in];
            ++pOut;
            *pOut=dmapU2M3[1+in];
            ++pOut;
            *pOut=dmapU2M3[2+in];
            ++pOut;
          }
        } else {
          *pOut=dmapU2M2[in];
          ++pOut;
          if (dmapU2M2[1+in] == 0x00) {
            if (*pOut == subS) {
              in>>=1;
              if (in != cd->srcSubS)
                ++numS;
            }
          } else {
            *pOut=dmapU2M2[1+in];
            ++pOut;
            if (memcmp(pOut-2, pSubD, 2) == 0) {
              in>>=1;
              if (in != cd->srcSubD) {
                ++numS;
                ++rc;
              }
            }
          }
        }
      }
    }
    *outBytesLeft-=(pOut-*outBuf);
    *inBytesLeft=inLen;
    *outBuf=pOut;
    *inBuf=pIn;
    *numSub+=numS;
    return 0;

  } else if (cd->cnv_dmap->codingSchema == DMAP_S2U) {
    /* use uchar * instead of UniChar to avoid memcpy */
    register uchar *    dmapD12U=(uchar *) (cd->cnv_dmap->dmapD12U);
    register int        inLen=*inBytesLeft;
    register char *     pOut=*outBuf;
    register char *     pIn=*inBuf;
    register int        offset;
    register char *     pLastOutBuf = *outBuf + *outBytesLeft - 1;
    register size_t     numS=0;
    while (0 < inLen) {
      if (pLastOutBuf < pOut)
        break;
      if (*pIn == 0x00) {
        *pOut=0x00;
        ++pOut;
        *pOut=0x00;
        ++pOut;
        ++pIn;
        --inLen;
      } else {
        offset=*pIn;
        offset<<=1;
        *pOut=dmapD12U[offset];
        ++pOut;
        *pOut=dmapD12U[offset+1];
        ++pOut;
        if (dmapD12U[offset]   == 0x00) {
          if (dmapD12U[offset+1] == 0x1A) {
            if (*pIn != cd->srcSubS)
              ++numS;
          } else if (dmapD12U[offset+1] == 0x00) {
            pOut-=2;
            *outBytesLeft-=(pOut-*outBuf);
            *inBytesLeft=inLen;
            *outBuf=pOut;
            *inBuf=pIn;
            *numSub+=numS;
            return -1;
          }
        }
        ++pIn;
        --inLen;
      }
    }
    *outBytesLeft-=(pOut-*outBuf);
    *inBytesLeft=inLen;
    *outBuf=pOut;
    *inBuf=pIn;
    *numSub+=numS;
    return 0;

  } else if (cd->cnv_dmap->codingSchema == DMAP_S28) {
    /* use uchar * instead of UniChar to avoid memcpy */
    register uchar *    dmapD12U=(uchar *) (cd->cnv_dmap->dmapD12U);
    register int        inLen=*inBytesLeft;
    register char *     pOut=*outBuf;
    register char *     pIn=*inBuf;
    register int        offset;
    register char *     pLastOutBuf = *outBuf + *outBytesLeft - 1;
    register size_t     numS=0;
    register UniChar    in;     /* copy part of U28 */
    while (0 < inLen) {
      if (pLastOutBuf < pOut)
        break;
      if (*pIn == 0x00) {
        *pOut=0x00;
        ++pOut;
        ++pIn;
        --inLen;
      } else {
        offset=*pIn;
        offset<<=1;
        in=dmapD12U[offset];
        in<<=8;
        in+=dmapD12U[offset+1];
        if ((in & 0xFF80) == 0x0000) {     /* U28: in & 0b1111111110000000 == 0x0000 */
          if (in == 0x000) {
            errno=EILSEQ;  /* 116 */
            *outBytesLeft-=(pOut-*outBuf);
            *inBytesLeft=inLen;
            *outBuf=pOut;
            *inBuf=pIn;
            *numSub+=numS;
            return -1;
          }
          *pOut=in;
          ++pOut;
        } else if ((in & 0xF800) == 0x0000) {     /* in & 0b1111100000000000 == 0x0000 */
          register uchar        byte;
          in>>=6;
          in&=0x001F;   /* 0b0000000000011111 */
          in|=0x00C0;   /* 0b0000000011000000 */
          *pOut=in;
          ++pOut;
          byte=dmapD12U[offset+1];
          byte&=0x3F;   /* 0b00111111; */
          byte|=0x80;   /* 0b10000000; */
          *pOut=byte;
          ++pOut;
        } else if ((in & 0xFC00) == 0xD800) {    /* There should not be no surrogate character in SBCS. */
          *pOut=0xEF;
          ++pOut;
          *pOut=0xBF;
          ++pOut;
          *pOut=0xBD;
          ++pOut;
        } else {
          register uchar        byte;
          register uchar        work;
          byte=dmapD12U[offset];
          byte>>=4;
          byte|=0xE0;   /* 0b11100000; */
          *pOut=byte;
          ++pOut;

          byte=dmapD12U[offset];
          byte<<=2;
          work=dmapD12U[offset+1];
          work>>=6;
          byte|=work;
          byte&=0x3F;   /* 0b00111111; */
          byte|=0x80;   /* 0b10000000; */
          *pOut=byte;
          ++pOut;

          byte=dmapD12U[offset+1];
          byte&=0x3F;   /* 0b00111111; */
          byte|=0x80;   /* 0b10000000; */
          *pOut=byte;
          ++pOut;
        }
        /* end of U28 */
        if (dmapD12U[offset]   == 0x00) {
          if (dmapD12U[offset+1] == 0x1A) {
            if (*pIn != cd->srcSubS)
              ++numS;
          }
        }
        ++pIn;
        --inLen;
      }
    }
    *outBytesLeft-=(pOut-*outBuf);
    *inBytesLeft=inLen;
    *outBuf=pOut;
    *inBuf=pIn;
    *numSub+=numS;
    return 0;

  } else if (cd->cnv_dmap->codingSchema == DMAP_U2S) {
    register uchar *    dmapU2S=cd->cnv_dmap->dmapU2S;
    register int        inLen=*inBytesLeft;
    register char *     pOut=*outBuf;
    register char *     pIn=*inBuf;
    register char *     pLastOutBuf = *outBuf + *outBytesLeft - 1;
    register char       subS=cd->subS;
    register size_t     numS=0;
    while (0 < inLen) {
      register uint32_t       in;
      if (inLen == 1) {
        errno=EINVAL;   /* 22 */

        *inBytesLeft=inLen;
        *outBytesLeft-=(pOut-*outBuf);
        *outBuf=pOut;
        *inBuf=pIn;
        return -1;
      }
      if (pLastOutBuf < pOut)
        break;
      in=pIn[0];
      in<<=8;
      in+=pIn[1];
      if (in == 0x0000) {
        *pOut=0x00;
      } else {
        if ((*pOut=dmapU2S[in]) == 0x00) {
          *pOut=subS;
          ++numS;
          errno=EINVAL;  /* 22 */
        } else if (*pOut == subS) {
          if (in != cd->srcSubS)
            ++numS;
        }
      }
      ++pOut;
      pIn+=2;
      inLen-=2;
    }
    *outBytesLeft-=(pOut-*outBuf);
    *inBytesLeft=inLen;
    *outBuf=pOut;
    *inBuf=pIn;
    *numSub+=numS;
    return numS;

  } else if (cd->cnv_dmap->codingSchema == DMAP_T2S) {
    register uchar *    dmapU2S=cd->cnv_dmap->dmapU2S;
    register int        inLen=*inBytesLeft;
    register char *     pOut=*outBuf;
    register char *     pIn=*inBuf;
    register char *     pLastOutBuf = *outBuf + *outBytesLeft - 1;
    register char       subS=cd->subS;
    register size_t     numS=0;
    while (0 < inLen) {
      register uint32_t       in;
      if (inLen == 1) {
        errno=EINVAL;   /* 22 */

        *inBytesLeft=inLen-1;
        *outBytesLeft-=(pOut-*outBuf);
        *outBuf=pOut;
        *inBuf=pIn;
        ++numS;
        *numSub+=numS;
        return 0;
      }
      if (pLastOutBuf < pOut)
        break;
      in=pIn[0];
      in<<=8;
      in+=pIn[1];
      if (in == 0x0000) {
        *pOut=0x00;

      } else if (0xD800 <= in && in <=  0xDFFF) {     /* 0xD800-0xDFFF, surrogate first and second values */
        if (0xDC00 <= in ) {
          errno=EINVAL;   /* 22 */
          *inBytesLeft=inLen-1;
          *outBytesLeft-=(pOut-*outBuf);
          *outBuf=pOut;
          *inBuf=pIn;
          return -1;
          
        } else if (inLen < 4) {
          errno=EINVAL;   /* 22 */
          *inBytesLeft=inLen-2;
          *outBytesLeft-=(pOut-*outBuf);
          *outBuf=pOut;
          *inBuf=pIn+2;
          return -1;
          
        } else {
          register uint32_t       in2;
          in2=pIn[2];
          in2<<=8;
          in2+=pIn[3];
          if (0xDC00 <= in2 && in2 <= 0xDFFF) {   /* second surrogate character =0xDC00 - 0xDFFF*/
            *pOut=subS;
            ++numS;
            pIn+=4;
          } else {
            errno=EINVAL;   /* 22 */
            *inBytesLeft=inLen-1;
            *outBytesLeft-=(pOut-*outBuf);
            *outBuf=pOut;
            *inBuf=pIn;
            return -1;
          }
        }
      } else {
        if ((*pOut=dmapU2S[in]) == 0x00) {
          *pOut=subS;
          ++numS;
          errno=EINVAL;  /* 22 */
        } else if (*pOut == subS) {
          if (in != cd->srcSubS)
            ++numS;
        }
      }
      ++pOut;
      pIn+=2;
      inLen-=2;
    }
    *outBytesLeft-=(pOut-*outBuf);
    *inBytesLeft=inLen;
    *outBuf=pOut;
    *inBuf=pIn;
    *numSub+=numS;
    return 0;

  } else if (cd->cnv_dmap->codingSchema == DMAP_82S) {
    register uchar *    dmapU2S=cd->cnv_dmap->dmapU2S;
    register int        inLen=*inBytesLeft;
    register char *     pOut=*outBuf;
    register char *     pIn=*inBuf;
    register char *     pLastOutBuf = *outBuf + *outBytesLeft - 1;
    register char       subS=cd->subS;
    register size_t     numS=0;
    while (0 < inLen) {
      register uint32_t         in;
      uint32_t                  in2;  /* The second surrogate value */
      if (pLastOutBuf < pOut)
        break;
      /* convert from UTF-8 to UCS-2 */
      if (*pIn == 0x00) {
        in=0x0000;
        ++pIn;
        --inLen;
      } else {                          /* 82U: */
        register uchar byte1=*pIn;
        if ((byte1 & 0x80) == 0x00) {         /* if (byte1 & 0b10000000 == 0b00000000) { */
          /* 1 bytes sequence:  0xxxxxxx => 00000000 0xxxxxxx*/
          in=byte1;
          ++pIn;
          --inLen;
        } else if ((byte1 & 0xE0) == 0xC0) {  /* (byte1 & 0b11100000 == 0b11000000) { */
          if (inLen < 2) {
            errno=EINVAL;  /* 22 */
            *outBytesLeft-=(pOut-*outBuf);
            *inBytesLeft=inLen;
            *outBuf=pOut;
            *inBuf=pIn;
            *numSub+=numS;
            return -1;
          }
          if (byte1 == 0xC0 || byte1 == 0xC1) {  /* invalid sequence */
            errno=EILSEQ;  /* 116 */
            *outBytesLeft-=(pOut-*outBuf);
            *inBytesLeft=inLen;
            *outBuf=pOut;
            *inBuf=pIn;
            *numSub+=numS;
            return -1;
          }
          /* 2 bytes sequence: 
             110yyyyy 10xxxxxx => 00000yyy yyxxxxxx */
          register uchar byte2;
          ++pIn;
          byte2=*pIn;
          if ((byte2 & 0xC0) == 0x80) {       /* byte2 & 0b11000000 == 0b10000000) { */
            register uchar work=byte1;
            work<<=6;
            byte2&=0x3F;      /* 0b00111111; */
            byte2|=work;
              
            byte1&=0x1F;      /* 0b00011111; */
            byte1>>=2;
            in=byte1;
            in<<=8;
            in+=byte2;
            inLen-=2;
            ++pIn;
          } else { /* invalid sequence */
            errno=EILSEQ;  /* 116 */
            *outBytesLeft-=(pOut-*outBuf);
            *inBytesLeft=inLen;
            *outBuf=pOut;
            *inBuf=pIn-1;
            *numSub+=numS;
            return -1;
          }
        } else if ((byte1 & 0xF0) == 0xE0) {    /* byte1 & 0b11110000 == 0b11100000 */
          /* 3 bytes sequence: 
             1110zzzz 10yyyyyy 10xxxxxx => zzzzyyyy yyxxxxxx */
          register uchar byte2;
          register uchar byte3;
          if (inLen < 3) {
            if (inLen == 2 && (pIn[1]  & 0xC0) != 0x80)
              errno=EILSEQ;  /* 116 */
            else
              errno=EINVAL;  /* 22 */
            *outBytesLeft-=(pOut-*outBuf);
            *inBytesLeft=inLen;
            *outBuf=pOut;
            *inBuf=pIn;
            *numSub+=numS;
            return -1;
          }
          ++pIn;
          byte2=*pIn;
          ++pIn;
          byte3=*pIn;
          if ((byte2 & 0xC0) != 0x80 ||
              (byte3 & 0xC0) != 0x80 ||
              (byte1 == 0xE0 && byte2 < 0xA0)) { /* invalid sequence, only 0xA0-0xBF allowed after 0xE0 */
            errno=EILSEQ;  /* 116 */
            *outBytesLeft-=(pOut-*outBuf);
            *inBytesLeft=inLen;
            *outBuf=pOut;
            *inBuf=pIn-2;
            *numSub+=numS;
            return -1;
          }
          {
            register uchar work=byte2;
            work<<=6;
            byte3&=0x3F;      /* 0b00111111; */
            byte3|=work;
              
            byte2&=0x3F;      /* 0b00111111; */
            byte2>>=2;

            byte1<<=4;
            in=byte1 | byte2;;
            in<<=8;
            in+=byte3;
            inLen-=3;
            ++pIn;
          }
        } else if ((0xF0 <= byte1 && byte1 <= 0xF4) ||    /* (bytes1 & 11111000) == 0x1110000 */
                   ((byte1&=0xF7) && 0xF0 <= byte1 && byte1 <= 0xF4)) { /* minic iconv() behavior */
          /* 4 bytes sequence
             11110uuu 10uuzzzz 10yyyyyy 10xxxxxx => 110110ww wwzzzzyy 110111yy yyxxxxxx
             where uuuuu = wwww + 1 */
          register uchar byte2;
          register uchar byte3;
          register uchar byte4;
          if (inLen < 4) {
            if ((inLen >= 2 && (pIn[1]  & 0xC0) != 0x80) ||
                (inLen >= 3 && (pIn[2]  & 0xC0) != 0x80) ||
                (cd->toCcsid == 13488) )
              errno=EILSEQ;  /* 116 */
            else
              errno=EINVAL;  /* 22 */
            *outBytesLeft-=(pOut-*outBuf);
            *inBytesLeft=inLen;
            *outBuf=pOut;
            *inBuf=pIn;
            *numSub+=numS;
            return -1;
          }
          ++pIn;
          byte2=*pIn;
          ++pIn;
          byte3=*pIn;
          ++pIn;
          byte4=*pIn;
          if ((byte2 & 0xC0) == 0x80 &&         /* byte2 & 0b11000000 == 0b10000000 */
              (byte3 & 0xC0) == 0x80 &&         /* byte3 & 0b11000000 == 0b10000000 */
              (byte4 & 0xC0) == 0x80) {         /* byte4 & 0b11000000 == 0b10000000 */
            register uchar work=byte2;
            if (byte1 == 0xF0 && byte2 < 0x90) {
              errno=EILSEQ;  /* 116 */
              *outBytesLeft-=(pOut-*outBuf);
              *inBytesLeft=inLen;
              *outBuf=pOut;
              *inBuf=pIn-3;
              *numSub+=numS;
              return -1;
              /* iconv() returns 0 for 0xF4908080 and convert to 0x00 
            } else if (byte1 == 0xF4 && byte2 > 0x8F) {
              errno=EINVAL;
              *outBytesLeft-=(pOut-*outBuf);
              *inBytesLeft=inLen;
              *outBuf=pOut;
              *inBuf=pIn-3;
              *numSub+=numS;
              return -1;
              */
            }

            work&=0x30;       /* 0b00110000; */
            work>>=4;
            byte1&=0x07;      /* 0b00000111; */
            byte1<<=2;
            byte1+=work;      /* uuuuu */
            --byte1;          /* wwww  */

            work=byte1 & 0x0F;
            work>>=2;
            work+=0xD8;       /* 0b11011011; */
            in=work;
            in<<=8;

            byte1<<=6;
            byte2<<=2;
            byte2&=0x3C;      /* 0b00111100; */
            work=byte3;
            work>>=4;
            work&=0x03;       /* 0b00000011; */
            work|=byte1;
            work|=byte2;
            in+=work;

            work=byte3;
            work>>=2;
            work&=0x03;       /* 0b00000011; */
            work|=0xDC;       /* 0b110111xx; */
            in2=work;
            in2<<=8;

            byte3<<=6;
            byte4&=0x3F;      /* 0b00111111; */
            byte4|=byte3;
            in2+=byte4;
            inLen-=4;
            ++pIn;
          } else { /* invalid sequence */
            errno=EILSEQ;  /* 116 */
            *outBytesLeft-=(pOut-*outBuf);
            *inBytesLeft=inLen;
            *outBuf=pOut;
            *inBuf=pIn-3;
            *numSub+=numS;
            return -1;
          }
        } else if ((byte1 & 0xF0) == 0xF0) { /* minic iconv() behavior */
          if (inLen < 4 ||
              pIn[1] < 0x80 || 0xBF < pIn[1] ||
              pIn[2] < 0x80 || 0xBF < pIn[2] ||
              pIn[3] < 0x80 || 0xBF < pIn[3] ) {
            if (inLen == 1)
              errno=EINVAL;  /* 22 */
            else if (inLen == 2 && (pIn[1]  & 0xC0) != 0x80)
              errno=EILSEQ;  /* 116 */
            else if (inLen == 3 && ((pIn[1]  & 0xC0) != 0x80 || (pIn[2] & 0xC0) != 0x80))
              errno=EILSEQ;  /* 116 */
            else if (inLen >= 4 && ((pIn[1]  & 0xC0) != 0x80 || (pIn[2] & 0xC0) != 0x80 || (pIn[3] & 0xC0) != 0x80))
              errno=EILSEQ;  /* 116 */
            else
              errno=EINVAL;  /* 22 */

            *outBytesLeft-=(pOut-*outBuf);
            *inBytesLeft=inLen;
            *outBuf=pOut;
            *inBuf=pIn;
            *numSub+=numS;
            return -1;
          } else {
            *pOut=subS;   /* Though returns replacement character, which iconv() does not return. */
            ++pOut;
            ++numS;
            pIn+=4;
            inLen-=4;
            /* UTF-8_IBM-850 0xF0908080 : converted value does not match, iconv=0x00, dmap=0x7F
               UTF-8_IBM-850 0xF0908081 : converted value does not match, iconv=0x01, dmap=0x7F
               UTF-8_IBM-850 0xF0908082 : converted value does not match, iconv=0x02, dmap=0x7F
               UTF-8_IBM-850 0xF0908083 : converted value does not match, iconv=0x03, dmap=0x7F 
               ....
               UTF-8_IBM-850 0xF09081BE : converted value does not match, iconv=0x7E, dmap=0x7F
               UTF-8_IBM-850 0xF09081BF : converted value does not match, iconv=0x1C, dmap=0x7F
               UTF-8_IBM-850 0xF09082A0 : converted value does not match, iconv=0xFF, dmap=0x7F
               UTF-8_IBM-850 0xF09082A1 : converted value does not match, iconv=0xAD, dmap=0x7F
               ....
            */
            continue;
            /* iconv() returns 0 with strange 1 byte converted values */
          }

        } else { /* invalid sequence */
          errno=EILSEQ;  /* 116 */
          *outBytesLeft-=(pOut-*outBuf);
          *inBytesLeft=inLen;
          *outBuf=pOut;
          *inBuf=pIn;
          *numSub+=numS;
          return -1;
        }
      }
      /* end of UTF-8 to UCS-2 */
      if (in == 0x0000) {
        *pOut=0x00;
      } else {
        if ((*pOut=dmapU2S[in]) == 0x00) {
          *pOut=subS;
          ++numS;
          errno=EINVAL;  /* 22 */
        } else if (*pOut == subS) {
          if (in != cd->srcSubS) {
            ++numS;
          }
        }
      }
      ++pOut;
    }
    *outBytesLeft-=(pOut-*outBuf);
    *inBytesLeft=inLen;
    *outBuf=pOut;
    *inBuf=pIn;
    *numSub+=numS;
    return 0;

  } else if (cd->cnv_dmap->codingSchema == DMAP_D2U) {
    /* use uchar * instead of UniChar to avoid memcpy */
    register uchar *    dmapD12U=(uchar *) (cd->cnv_dmap->dmapD12U);
    register uchar *    dmapD22U=(uchar *) (cd->cnv_dmap->dmapD22U);
    register int        inLen=*inBytesLeft;
    register char *     pOut=*outBuf;
    register char *     pIn=*inBuf;
    register int        offset;
    register char *     pLastOutBuf = *outBuf + *outBytesLeft - 1;
    register size_t   numS=0;
    while (0 < inLen) {
      if (pLastOutBuf < pOut)
        break;
      if (*pIn == 0x00) {
        *pOut=0x00;
        ++pOut;
        *pOut=0x00;
        ++pOut;
        ++pIn;
        --inLen;
      } else {
        offset=*pIn;
        offset<<=1;
        if (dmapD12U[offset]   == 0x00 &&
            dmapD12U[offset+1] == 0x00) {         /* DBCS */
          if (inLen < 2) {
            if (*pIn == 0x80 || *pIn == 0xFF ||
                (cd->fromCcsid == 943 && (*pIn == 0x85 || *pIn == 0x86 || *pIn == 0xA0 || *pIn == 0xEB || *pIn == 0xEC || *pIn == 0xEF || *pIn == 0xFD || *pIn == 0xFE)) ||
                (cd->fromCcsid == 932 && (*pIn == 0x85 || *pIn == 0x86 || *pIn == 0x87 || *pIn == 0xEB || *pIn == 0xEC || *pIn == 0xED || *pIn == 0xEE || *pIn == 0xEF)) ||
                (cd->fromCcsid == 1381 && ((0x85 <= *pIn && *pIn <= 0x8B) || (0xAA <= *pIn && *pIn <= 0xAF) || (0xF8 <= *pIn && *pIn <= 0xFE))))
              errno=EILSEQ;  /* 116 */
            else
              errno=EINVAL;  /* 22 */
            *outBytesLeft-=(pOut-*outBuf);
            *inBytesLeft=inLen;
            *outBuf=pOut;
            *inBuf=pIn;
            return -1;
          }
          offset-=0x100;
          ++pIn;
          offset<<=8;
          offset+=(*pIn * 2);
          if (dmapD22U[offset] == 0x00 &&
              dmapD22U[offset+1] == 0x00) {
            errno=EILSEQ;  /* 116 */
            *outBytesLeft-=(pOut-*outBuf);
            *inBytesLeft=inLen;
            *outBuf=pOut;
            *inBuf=pIn-1;
            return -1;
          }
          *pOut=dmapD22U[offset];
          ++pOut;
          *pOut=dmapD22U[offset+1];
          ++pOut;
          if (dmapD22U[offset] == 0xFF &&
              dmapD22U[offset+1] == 0xFD) {
            if (pIn[-1] * 0x100 + pIn[0] != cd->srcSubD)
              ++numS;
          }
          ++pIn;
          inLen-=2;
        } else {  /* SBCS */
          *pOut=dmapD12U[offset];
          ++pOut;
          *pOut=dmapD12U[offset+1];
          ++pOut;
          if (dmapD12U[offset]   == 0x00 &&
              dmapD12U[offset+1] == 0x1A) {
            if (*pIn != cd->srcSubS)
              ++numS;
          }
          ++pIn;
          --inLen;
        }
      }
    }
    *outBytesLeft-=(pOut-*outBuf);
    *inBytesLeft=inLen;
    *outBuf=pOut;
    *inBuf=pIn;
    *numSub+=numS;
    return 0;

  } else if (cd->cnv_dmap->codingSchema == DMAP_D28) {
    /* use uchar * instead of UniChar to avoid memcpy */
    register uchar *    dmapD12U=(uchar *) (cd->cnv_dmap->dmapD12U);
    register uchar *    dmapD22U=(uchar *) (cd->cnv_dmap->dmapD22U);
    register int        inLen=*inBytesLeft;
    register char *     pOut=*outBuf;
    register char *     pIn=*inBuf;
    register int        offset;
    register char *     pLastOutBuf = *outBuf + *outBytesLeft - 1;
    register size_t     numS=0;
    register UniChar    in;     /* copy part of U28 */
    register UniChar    ucs2;
    while (0 < inLen) {
      if (pLastOutBuf < pOut)
        break;
      if (*pIn == 0x00) {
        *pOut=0x00;
        ++pOut;
        ++pIn;
        --inLen;
      } else {
        offset=*pIn;
        offset<<=1;
        if (dmapD12U[offset]   == 0x00 &&
            dmapD12U[offset+1] == 0x00) {         /* DBCS */
          if (inLen < 2) {
            if (*pIn == 0x80 || *pIn == 0xFF ||
                (cd->fromCcsid == 943 && (*pIn == 0x85 || *pIn == 0x86 || *pIn == 0xA0 || *pIn == 0xEB || *pIn == 0xEC || *pIn == 0xEF || *pIn == 0xFD || *pIn == 0xFE)) ||
                (cd->fromCcsid == 932 && (*pIn == 0x85 || *pIn == 0x86 || *pIn == 0x87 || *pIn == 0xEB || *pIn == 0xEC || *pIn == 0xED || *pIn == 0xEE || *pIn == 0xEF)) ||
                (cd->fromCcsid == 1381 && ((0x85 <= *pIn && *pIn <= 0x8B) || (0xAA <= *pIn && *pIn <= 0xAF) || (0xF8 <= *pIn && *pIn <= 0xFE))))
              errno=EILSEQ;  /* 116 */
            else
              errno=EINVAL;  /* 22 */
            *outBytesLeft-=(pOut-*outBuf);
            *inBytesLeft=inLen;
            *outBuf=pOut;
            *inBuf=pIn;
            return -1;
          }
          offset-=0x100;
          ++pIn;
          offset<<=8;
          offset+=(*pIn * 2);
          if (dmapD22U[offset] == 0x00 &&
              dmapD22U[offset+1] == 0x00) {
            errno=EILSEQ;  /* 116 */
            *outBytesLeft-=(pOut-*outBuf);
            *inBytesLeft=inLen;
            *outBuf=pOut;
            *inBuf=pIn-1;
            return -1;
          }
          in=dmapD22U[offset];
          in<<=8;
          in+=dmapD22U[offset+1];
          ucs2=in;
          if (dmapD22U[offset] == 0xFF &&
              dmapD22U[offset+1] == 0xFD) {
            if (in != cd->srcSubD)
              ++numS;
          }
          ++pIn;
          inLen-=2;
        } else {  /* SBCS */
          in=dmapD12U[offset];
          in<<=8;
          in+=dmapD12U[offset+1];
          ucs2=in;
          if (dmapD12U[offset]   == 0x00 &&
              dmapD12U[offset+1] == 0x1A) {
            if (in != cd->srcSubS)
              ++numS;
          }
          ++pIn;
          --inLen;
        }
        if ((in & 0xFF80) == 0x0000) {     /* U28: in & 0b1111111110000000 == 0x0000 */
          *pOut=in;
          ++pOut;
        } else if ((in & 0xF800) == 0x0000) {     /* in & 0b1111100000000000 == 0x0000 */
          register uchar        byte;
          in>>=6;
          in&=0x001F;   /* 0b0000000000011111 */
          in|=0x00C0;   /* 0b0000000011000000 */
          *pOut=in;
          ++pOut;
          byte=ucs2;    /* dmapD12U[offset+1]; */
          byte&=0x3F;   /* 0b00111111; */
          byte|=0x80;   /* 0b10000000; */
          *pOut=byte;
          ++pOut;
        } else if ((in & 0xFC00) == 0xD800) {    /* There should not be no surrogate character in SBCS. */
          *pOut=0xEF;
          ++pOut;
          *pOut=0xBF;
          ++pOut;
          *pOut=0xBD;
          ++pOut;
        } else {
          register uchar        byte;
          register uchar        work;
          byte=(ucs2>>8);    /* dmapD12U[offset]; */
          byte>>=4;
          byte|=0xE0;   /* 0b11100000; */
          *pOut=byte;
          ++pOut;

          byte=(ucs2>>8);    /* dmapD12U[offset]; */
          byte<<=2;
          work=ucs2;    /* dmapD12U[offset+1]; */
          work>>=6;
          byte|=work;
          byte&=0x3F;   /* 0b00111111; */
          byte|=0x80;   /* 0b10000000; */
          *pOut=byte;
          ++pOut;

          byte=ucs2;    /* dmapD12U[offset+1]; */
          byte&=0x3F;   /* 0b00111111; */
          byte|=0x80;   /* 0b10000000; */
          *pOut=byte;
          ++pOut;
        }
        /* end of U28 */
      }
    }
    *outBytesLeft-=(pOut-*outBuf);
    *inBytesLeft=inLen;
    *outBuf=pOut;
    *inBuf=pIn;
    *numSub+=numS;
    return 0;

  } else if (cd->cnv_dmap->codingSchema == DMAP_U2D) {
    register uchar *    dmapU2D=cd->cnv_dmap->dmapU2D;
    register int        inLen=*inBytesLeft;
    register char *     pOut=*outBuf;
    register char *     pIn=*inBuf;
    register char *     pLastOutBuf = *outBuf + *outBytesLeft - 1;
    register char       subS=cd->subS;
    register char *     pSubD=(char *) &(cd->subD);
    register size_t     numS=0;
    while (0 < inLen) {
      register uint32_t       in;
      if (inLen == 1) {
        errno=EINVAL;   /* 22 */

        *inBytesLeft=inLen;
        *outBytesLeft-=(pOut-*outBuf);
        *outBuf=pOut;
        *inBuf=pIn;
        return -1;
      }
      if (pLastOutBuf < pOut)
        break;
      in=pIn[0];
      in<<=8;
      in+=pIn[1];
      if (in == 0x0000) {
        *pOut=0x00;
        ++pOut;
      } else {
        in<<=1;
        *pOut=dmapU2D[in];
        ++pOut;
        if (dmapU2D[in+1] == 0x00) {   /* SBCS */
          if (*pOut == subS) {
            if (in != cd->srcSubS)
              ++numS;
          }
        } else {
          *pOut=dmapU2D[in+1];
          ++pOut;
          if (dmapU2D[in]   == pSubD[0] &&
              dmapU2D[in+1] == pSubD[1]) {
            in>>=1;
            if (in != cd->srcSubD)
              ++numS;
          }
        }
      }
      pIn+=2;
      inLen-=2;
    }
    *outBytesLeft-=(pOut-*outBuf);
    *inBytesLeft=inLen;
    *outBuf=pOut;
    *inBuf=pIn;
    *numSub+=numS;
    return numS;        /* to minic iconv() behavior */

  } else if (cd->cnv_dmap->codingSchema == DMAP_T2D) {
    register uchar *    dmapU2D=cd->cnv_dmap->dmapU2D;
    register int        inLen=*inBytesLeft;
    register char *     pOut=*outBuf;
    register char *     pIn=*inBuf;
    register char *     pLastOutBuf = *outBuf + *outBytesLeft - 1;
    register char       subS=cd->subS;
    register char *     pSubD=(char *) &(cd->subD);
    register size_t     numS=0;
    while (0 < inLen) {
      register uint32_t       in;
      if (inLen == 1) {
        errno=EINVAL;  /* 22 */
        *inBytesLeft=inLen-1;
        *outBytesLeft-=(pOut-*outBuf);
        *outBuf=pOut;
        *inBuf=pIn;
        ++numS;
        *numSub+=numS;
        return 0;
      }
      if (pLastOutBuf < pOut)
        break;
      in=pIn[0];
      in<<=8;
      in+=pIn[1];
      if (in == 0x0000) {
        *pOut=0x00;
        ++pOut;
      } else if (0xD800 <= in && in <= 0xDBFF) { /* first byte of surrogate */
        errno=EINVAL;   /* 22 */
        *inBytesLeft=inLen-2;
        *outBytesLeft-=(pOut-*outBuf);
        *outBuf=pOut;
        *inBuf=pIn+2;
        ++numS;
        *numSub+=numS;
        return -1;
        
      } else if (0xDC00 <= in && in <= 0xDFFF) { /* second byte of surrogate */
        errno=EINVAL;   /* 22 */
        *inBytesLeft=inLen-1;
        *outBytesLeft-=(pOut-*outBuf);
        *outBuf=pOut;
        *inBuf=pIn;
        ++numS;
        *numSub+=numS;
        return -1;
        
      } else {
        in<<=1;
        *pOut=dmapU2D[in];
        ++pOut;
        if (dmapU2D[in+1] == 0x00) {   /* SBCS */
          if (*pOut == subS) {
            if (in != cd->srcSubS)
              ++numS;
          }
        } else {
          *pOut=dmapU2D[in+1];
          ++pOut;
          if (dmapU2D[in]   == pSubD[0] &&
              dmapU2D[in+1] == pSubD[1]) {
            in>>=1;
            if (in != cd->srcSubD)
              ++numS;
          }
        }
      }
      pIn+=2;
      inLen-=2;
    }
    *outBytesLeft-=(pOut-*outBuf);
    *inBytesLeft=inLen;
    *outBuf=pOut;
    *inBuf=pIn;
    *numSub+=numS;
    return 0;        /* to minic iconv() behavior */

  } else if (cd->cnv_dmap->codingSchema == DMAP_82D) {
    register uchar *    dmapU2D=cd->cnv_dmap->dmapU2D;
    register int        inLen=*inBytesLeft;
    register char *     pOut=*outBuf;
    register char *     pIn=*inBuf;
    register char *     pLastOutBuf = *outBuf + *outBytesLeft - 1;
    register char       subS=cd->subS;
    register char *     pSubD=(char *) &(cd->subD);
    register size_t     numS=0;
    while (0 < inLen) {
      register uint32_t in;
      uint32_t          in2;
      if (pLastOutBuf < pOut)
        break;
      /* convert from UTF-8 to UCS-2 */
      if (*pIn == 0x00) {
        in=0x0000;
        ++pIn;
        --inLen;
      } else {                          /* 82U: */
        register uchar byte1=*pIn;
        if ((byte1 & 0x80) == 0x00) {         /* if (byte1 & 0b10000000 == 0b00000000) { */
          /* 1 bytes sequence:  0xxxxxxx => 00000000 0xxxxxxx*/
          in=byte1;
          ++pIn;
          --inLen;
        } else if ((byte1 & 0xE0) == 0xC0) {  /* (byte1 & 0b11100000 == 0b11000000) { */
          if (inLen < 2) {
            errno=EINVAL;  /* 22 */
            *outBytesLeft-=(pOut-*outBuf);
            *inBytesLeft=inLen;
            *outBuf=pOut;
            *inBuf=pIn;
            *numSub+=numS;
            return -1;
          }
          if (byte1 == 0xC0 || byte1 == 0xC1) {  /* invalid sequence */
            errno=EILSEQ;  /* 116 */
            *outBytesLeft-=(pOut-*outBuf);
            *inBytesLeft=inLen;
            *outBuf=pOut;
            *inBuf=pIn;
            *numSub+=numS;
            return -1;
          }
          /* 2 bytes sequence: 
             110yyyyy 10xxxxxx => 00000yyy yyxxxxxx */
          register uchar byte2;
          ++pIn;
          byte2=*pIn;
          if ((byte2 & 0xC0) == 0x80) {       /* byte2 & 0b11000000 == 0b10000000) { */
            register uchar work=byte1;
            work<<=6;
            byte2&=0x3F;      /* 0b00111111; */
            byte2|=work;
              
            byte1&=0x1F;      /* 0b00011111; */
            byte1>>=2;
            in=byte1;
            in<<=8;
            in+=byte2;
            inLen-=2;
            ++pIn;
          } else { /* invalid sequence */
            errno=EILSEQ;  /* 116 */
            *outBytesLeft-=(pOut-*outBuf);
            *inBytesLeft=inLen;
            *outBuf=pOut;
            *inBuf=pIn-1;
            *numSub+=numS;
            return -1;
          }
        } else if ((byte1 & 0xF0) == 0xE0) {    /* byte1 & 0b11110000 == 0b11100000 */
          /* 3 bytes sequence: 
             1110zzzz 10yyyyyy 10xxxxxx => zzzzyyyy yyxxxxxx */
          register uchar byte2;
          register uchar byte3;
          if (inLen < 3) {
            if (inLen == 2 && (pIn[1]  & 0xC0) != 0x80)
              errno=EILSEQ;  /* 116 */
            else
              errno=EINVAL;  /* 22 */
            *outBytesLeft-=(pOut-*outBuf);
            *inBytesLeft=inLen;
            *outBuf=pOut;
            *inBuf=pIn;
            *numSub+=numS;
            return -1;
          }
          ++pIn;
          byte2=*pIn;
          ++pIn;
          byte3=*pIn;
          if ((byte2 & 0xC0) != 0x80 ||
              (byte3 & 0xC0) != 0x80 ||
              (byte1 == 0xE0 && byte2 < 0xA0)) { /* invalid sequence, only 0xA0-0xBF allowed after 0xE0 */
            errno=EILSEQ;  /* 116 */
            *outBytesLeft-=(pOut-*outBuf);
            *inBytesLeft=inLen;
            *outBuf=pOut;
            *inBuf=pIn-2;
            *numSub+=numS;
            return -1;
          }
          {
            register uchar work=byte2;
            work<<=6;
            byte3&=0x3F;      /* 0b00111111; */
            byte3|=work;
              
            byte2&=0x3F;      /* 0b00111111; */
            byte2>>=2;

            byte1<<=4;
            in=byte1 | byte2;;
            in<<=8;
            in+=byte3;
            inLen-=3;
            ++pIn;
          }
        } else if ((0xF0 <= byte1 && byte1 <= 0xF4)) {    /* (bytes1 & 11111000) == 0x1110000 */
          /* 4 bytes sequence
             11110uuu 10uuzzzz 10yyyyyy 10xxxxxx => 110110ww wwzzzzyy 110111yy yyxxxxxx
             where uuuuu = wwww + 1 */
          register uchar byte2;
          register uchar byte3;
          register uchar byte4;
          if (inLen < 4) {
            if ((inLen >= 2 && (pIn[1]  & 0xC0) != 0x80) ||
                (inLen >= 3 && (pIn[2]  & 0xC0) != 0x80) ||
                (cd->toCcsid == 13488) )
              errno=EILSEQ;  /* 116 */
            else
              errno=EINVAL;  /* 22 */
            *outBytesLeft-=(pOut-*outBuf);
            *inBytesLeft=inLen;
            *outBuf=pOut;
            *inBuf=pIn;
            *numSub+=numS;
            return -1;
          }
          ++pIn;
          byte2=*pIn;
          ++pIn;
          byte3=*pIn;
          ++pIn;
          byte4=*pIn;
          if ((byte2 & 0xC0) == 0x80 &&         /* byte2 & 0b11000000 == 0b10000000 */
              (byte3 & 0xC0) == 0x80 &&         /* byte3 & 0b11000000 == 0b10000000 */
              (byte4 & 0xC0) == 0x80) {         /* byte4 & 0b11000000 == 0b10000000 */
            register uchar work=byte2;
            if (byte1 == 0xF0 && byte2 < 0x90) {
              errno=EILSEQ;  /* 116 */
              *outBytesLeft-=(pOut-*outBuf);
              *inBytesLeft=inLen;
              *outBuf=pOut;
              *inBuf=pIn-3;
              *numSub+=numS;
              return -1;
              /* iconv() returns 0 for 0xF4908080 and convert to 0x00 
            } else if (byte1 == 0xF4 && byte2 > 0x8F) {
              errno=EINVAL;
              *outBytesLeft-=(pOut-*outBuf);
              *inBytesLeft=inLen;
              *outBuf=pOut;
              *inBuf=pIn-3;
              *numSub+=numS;
              return -1;
              */
            }

            work&=0x30;       /* 0b00110000; */
            work>>=4;
            byte1&=0x07;      /* 0b00000111; */
            byte1<<=2;
            byte1+=work;      /* uuuuu */
            --byte1;          /* wwww  */

            work=byte1 & 0x0F;
            work>>=2;
            work+=0xD8;       /* 0b11011011; */
            in=work;
            in<<=8;

            byte1<<=6;
            byte2<<=2;
            byte2&=0x3C;      /* 0b00111100; */
            work=byte3;
            work>>=4;
            work&=0x03;       /* 0b00000011; */
            work|=byte1;
            work|=byte2;
            in+=work;

            work=byte3;
            work>>=2;
            work&=0x03;       /* 0b00000011; */
            work|=0xDC;       /* 0b110111xx; */
            in2=work;
            in2<<=8;

            byte3<<=6;
            byte4&=0x3F;      /* 0b00111111; */
            byte4|=byte3;
            in2+=byte4;
            inLen-=4;
            ++pIn;
#ifdef match_with_GBK
            if ((0xD800 == in && in2 < 0xDC80) ||
                (0xD840 == in && in2 < 0xDC80) ||
                (0xD880 == in && in2 < 0xDC80) ||
                (0xD8C0 == in && in2 < 0xDC80) ||
                (0xD900 == in && in2 < 0xDC80) ||
                (0xD940 == in && in2 < 0xDC80) ||
                (0xD980 == in && in2 < 0xDC80) ||
                (0xD9C0 == in && in2 < 0xDC80) ||
                (0xDA00 == in && in2 < 0xDC80) ||
                (0xDA40 == in && in2 < 0xDC80) ||
                (0xDA80 == in && in2 < 0xDC80) ||
                (0xDAC0 == in && in2 < 0xDC80) ||
                (0xDB00 == in && in2 < 0xDC80) ||
                (0xDB40 == in && in2 < 0xDC80) ||
                (0xDB80 == in && in2 < 0xDC80) ||
                (0xDBC0 == in && in2 < 0xDC80)) {
#else
              if ((0xD800 <= in  && in  <= 0xDBFF) &&
                  (0xDC00 <= in2 && in2 <= 0xDFFF)) {
#endif
              *pOut=subS;
              ++pOut;
              ++numS;
              continue;
            }
          } else { /* invalid sequence */
            errno=EILSEQ;  /* 116 */
            *outBytesLeft-=(pOut-*outBuf);
            *inBytesLeft=inLen;
            *outBuf=pOut;
            *inBuf=pIn-3;
            *numSub+=numS;
            return -1;
          }
        } else if (0xF5 <= byte1 && byte1 <= 0xFF) { /* minic iconv() behavior */
          if (inLen < 4 ||
              (inLen >= 4 && byte1 == 0xF8 && pIn[1] < 0x90) ||
              pIn[1] < 0x80 || 0xBF < pIn[1] ||
              pIn[2] < 0x80 || 0xBF < pIn[2] ||
              pIn[3] < 0x80 || 0xBF < pIn[3] ) {
            if (inLen == 1)
              errno=EINVAL;  /* 22 */
            else if (inLen == 2 && (pIn[1]  & 0xC0) != 0x80)
              errno=EILSEQ;  /* 116 */
            else if (inLen == 3 && ((pIn[1]  & 0xC0) != 0x80 || (pIn[2] & 0xC0) != 0x80))
              errno=EILSEQ;  /* 116 */
            else if (inLen >= 4 && (byte1 == 0xF8 || (pIn[1]  & 0xC0) != 0x80 || (pIn[2] & 0xC0) != 0x80 || (pIn[3] & 0xC0) != 0x80))
              errno=EILSEQ;  /* 116 */
            else
              errno=EINVAL;  /* 22 */

            *outBytesLeft-=(pOut-*outBuf);
            *inBytesLeft=inLen;
            *outBuf=pOut;
            *inBuf=pIn;
            *numSub+=numS;
            return -1;
          } else if ((pIn[1] == 0x80 || pIn[1] == 0x90 || pIn[1] == 0xA0 || pIn[1] == 0xB0) && 
                     pIn[2] < 0x82) {
            *pOut=subS;   /* Though returns replacement character, which iconv() does not return. */
            ++pOut;
            ++numS;
            pIn+=4;
            inLen-=4;
            continue;
          } else {
            *pOut=pSubD[0];   /* Though returns replacement character, which iconv() does not return. */
            ++pOut;
            *pOut=pSubD[1];
            ++pOut;
            ++numS;
            pIn+=4;
            inLen-=4;
            continue;
            /* iconv() returns 0 with strange 1 byte converted values */
          }

        } else { /* invalid sequence */
          errno=EILSEQ;  /* 116 */
          *outBytesLeft-=(pOut-*outBuf);
          *inBytesLeft=inLen;
          *outBuf=pOut;
          *inBuf=pIn;
          *numSub+=numS;
          return -1;
        }
      }
      /* end of UTF-8 to UCS-2 */
      if (in == 0x0000) {
        *pOut=0x00;
        ++pOut;
      } else {
        in<<=1;
        *pOut=dmapU2D[in];
        ++pOut;
        if (dmapU2D[in+1] == 0x00) {   /* SBCS */
          if (dmapU2D[in] == subS) {
            in>>=1;
            if (in != cd->srcSubS)
              ++numS;
          }
        } else {
          *pOut=dmapU2D[in+1];
          ++pOut;
          if (dmapU2D[in]   == pSubD[0] &&
              dmapU2D[in+1] == pSubD[1]) {
            in>>=1;
            if (in != cd->srcSubD)
              ++numS;
          }
        }
      }
    }
    *outBytesLeft-=(pOut-*outBuf);
    *inBytesLeft=inLen;
    *outBuf=pOut;
    *inBuf=pIn;
    *numSub+=numS;
    return 0;

  } else if (cd->cnv_dmap->codingSchema == DMAP_82U) {
    /* See http://unicode.org/versions/corrigendum1.html */
    /* convert from UTF-8 to UTF-16 can cover all conversion from UTF-8 to UCS-2 */
    register int        inLen=*inBytesLeft;
    register char *     pOut=*outBuf;
    register char *     pIn=*inBuf;
    register char *     pLastOutBuf = *outBuf + *outBytesLeft - 1;
    register size_t     numS=0;
    while (0 < inLen) {
      if (pLastOutBuf < pOut)
        break;
      if (*pIn == 0x00) {
        *pOut=0x00;
        ++pOut;
        *pOut=0x00;
        ++pOut;
        ++pIn;
        --inLen;
      } else {                          /* 82U: */
        register uchar byte1=*pIn;
        if ((byte1 & 0x80) == 0x00) {         /* if (byte1 & 0b10000000 == 0b00000000) { */
          /* 1 bytes sequence:  0xxxxxxx => 00000000 0xxxxxxx*/
          *pOut=0x00;
          ++pOut;
          *pOut=byte1;
          ++pOut;
          ++pIn;
          --inLen;
        } else if ((byte1 & 0xE0) == 0xC0) {  /* (byte1 & 0b11100000 == 0b11000000) { */
          if (inLen < 2) {
            errno=EINVAL;  /* 22 */
            *outBytesLeft-=(pOut-*outBuf);
            *inBytesLeft=inLen;
            *outBuf=pOut;
            *inBuf=pIn;
            *numSub+=numS;
            return -1;
          }
          if (byte1 == 0xC0 || byte1 == 0xC1) {  /* invalid sequence */
            errno=EILSEQ;  /* 116 */
            *outBytesLeft-=(pOut-*outBuf);
            *inBytesLeft=inLen;
            *outBuf=pOut;
            *inBuf=pIn;
            *numSub+=numS;
            return -1;
          }
          /* 2 bytes sequence: 
             110yyyyy 10xxxxxx => 00000yyy yyxxxxxx */
          register uchar byte2;
          ++pIn;
          byte2=*pIn;
          if ((byte2 & 0xC0) == 0x80) {       /* byte2 & 0b11000000 == 0b10000000) { */
            register uchar work=byte1;
            work<<=6;
            byte2&=0x3F;      /* 0b00111111; */
            byte2|=work;
              
            byte1&=0x1F;      /* 0b00011111; */
            byte1>>=2;
            *pOut=byte1;
            ++pOut;
            *pOut=byte2;
            ++pOut;
            inLen-=2;
            ++pIn;
          } else { /* invalid sequence */
            errno=EILSEQ;  /* 116 */
            *outBytesLeft-=(pOut-*outBuf);
            *inBytesLeft=inLen;
            *outBuf=pOut;
            *inBuf=pIn-1;
            *numSub+=numS;
            return -1;
          }
        } else if ((byte1 & 0xF0) == 0xE0) {    /* byte1 & 0b11110000 == 0b11100000 */
          /* 3 bytes sequence: 
             1110zzzz 10yyyyyy 10xxxxxx => zzzzyyyy yyxxxxxx */
          register uchar byte2;
          register uchar byte3;
          if (inLen < 3) {
            if (inLen == 2 && (pIn[1]  & 0xC0) != 0x80)
              errno=EILSEQ;  /* 116 */
            else
              errno=EINVAL;  /* 22 */
            *outBytesLeft-=(pOut-*outBuf);
            *inBytesLeft=inLen;
            *outBuf=pOut;
            *inBuf=pIn;
            *numSub+=numS;
            return -1;
          }
          ++pIn;
          byte2=*pIn;
          ++pIn;
          byte3=*pIn;
          if ((byte2 & 0xC0) != 0x80 ||
              (byte3 & 0xC0) != 0x80 ||
              (byte1 == 0xE0 && byte2 < 0xA0)) { /* invalid sequence, only 0xA0-0xBF allowed after 0xE0 */
            errno=EILSEQ;  /* 116 */
            *outBytesLeft-=(pOut-*outBuf);
            *inBytesLeft=inLen;
            *outBuf=pOut;
            *inBuf=pIn-2;
            *numSub+=numS;
            return -1;
          }
          {
            register uchar work=byte2;
            work<<=6;
            byte3&=0x3F;      /* 0b00111111; */
            byte3|=work;
              
            byte2&=0x3F;      /* 0b00111111; */
            byte2>>=2;

            byte1<<=4;
            *pOut=byte1 | byte2;;
            ++pOut;
            *pOut=byte3;
            ++pOut;
            inLen-=3;
            ++pIn;
          }
        } else if ((0xF0 <= byte1 && byte1 <= 0xF4) ||    /* (bytes1 & 11111000) == 0x1110000 */
                   ((byte1&=0xF7) && 0xF0 <= byte1 && byte1 <= 0xF4)) { /* minic iconv() behavior */
          /* 4 bytes sequence
             11110uuu 10uuzzzz 10yyyyyy 10xxxxxx => 110110ww wwzzzzyy 110111yy yyxxxxxx
             where uuuuu = wwww + 1 */
          register uchar byte2;
          register uchar byte3;
          register uchar byte4;
          if (inLen < 4 || cd->toCcsid == 13488) {
            if ((inLen >= 2 && (pIn[1]  & 0xC0) != 0x80) ||
                (inLen >= 3 && (pIn[2]  & 0xC0) != 0x80) ||
                (cd->toCcsid == 13488) )
              errno=EILSEQ;  /* 116 */
            else
              errno=EINVAL;  /* 22 */
            *outBytesLeft-=(pOut-*outBuf);
            *inBytesLeft=inLen;
            *outBuf=pOut;
            *inBuf=pIn;
            *numSub+=numS;
            return -1;
          }
          ++pIn;
          byte2=*pIn;
          ++pIn;
          byte3=*pIn;
          ++pIn;
          byte4=*pIn;
          if ((byte2 & 0xC0) == 0x80 &&         /* byte2 & 0b11000000 == 0b10000000 */
              (byte3 & 0xC0) == 0x80 &&         /* byte3 & 0b11000000 == 0b10000000 */
              (byte4 & 0xC0) == 0x80) {         /* byte4 & 0b11000000 == 0b10000000 */
            register uchar work=byte2;
            if (byte1 == 0xF0 && byte2 < 0x90) {
              errno=EILSEQ;  /* 116 */
              *outBytesLeft-=(pOut-*outBuf);
              *inBytesLeft=inLen;
              *outBuf=pOut;
              *inBuf=pIn-3;
              *numSub+=numS;
              return -1;
            } else if (byte1 == 0xF4 && byte2 > 0x8F) {
              errno=EINVAL;  /* 22 */
              *outBytesLeft-=(pOut-*outBuf);
              *inBytesLeft=inLen;
              *outBuf=pOut;
              *inBuf=pIn-3;
              *numSub+=numS;
              return -1;
            }

            work&=0x30;       /* 0b00110000; */
            work>>=4;
            byte1&=0x07;      /* 0b00000111; */
            byte1<<=2;
            byte1+=work;      /* uuuuu */
            --byte1;          /* wwww  */

            work=byte1 & 0x0F;
            work>>=2;
            work+=0xD8;       /* 0b11011011; */
            *pOut=work;
            ++pOut;

            byte1<<=6;
            byte2<<=2;
            byte2&=0x3C;      /* 0b00111100; */
            work=byte3;
            work>>=4;
            work&=0x03;       /* 0b00000011; */
            work|=byte1;
            work|=byte2;
            *pOut=work;
            ++pOut;

            work=byte3;
            work>>=2;
            work&=0x03;       /* 0b00000011; */
            work|=0xDC;       /* 0b110111xx; */
            *pOut=work;
            ++pOut;

            byte3<<=6;
            byte4&=0x3F;      /* 0b00111111; */
            byte4|=byte3;
            *pOut=byte4;
            ++pOut;
            inLen-=4;
            ++pIn;
          } else { /* invalid sequence */
            errno=EILSEQ;  /* 116 */
            *outBytesLeft-=(pOut-*outBuf);
            *inBytesLeft=inLen;
            *outBuf=pOut;
            *inBuf=pIn-3;
            *numSub+=numS;
            return -1;
          }
        } else if ((byte1 & 0xF0) == 0xF0) {
          if (cd->toCcsid == 13488) {
            errno=EILSEQ;  /* 116 */
          } else {
            if (inLen == 1)
              errno=EINVAL;  /* 22 */
            else if (inLen == 2 && (pIn[1]  & 0xC0) != 0x80)
              errno=EILSEQ;  /* 116 */
            else if (inLen == 3 && ((pIn[1]  & 0xC0) != 0x80 || (pIn[2] & 0xC0) != 0x80))
              errno=EILSEQ;  /* 116 */
            else if (inLen >= 4 && ((pIn[1]  & 0xC0) != 0x80 || (pIn[2] & 0xC0) != 0x80 || (pIn[3] & 0xC0) != 0x80))
              errno=EILSEQ;  /* 116 */
            else
              errno=EINVAL;  /* 22 */
          }
          *outBytesLeft-=(pOut-*outBuf);
          *inBytesLeft=inLen;
          *outBuf=pOut;
          *inBuf=pIn;
          *numSub+=numS;
          return -1;

        } else { /* invalid sequence */
          errno=EILSEQ;  /* 116 */
          *outBytesLeft-=(pOut-*outBuf);
          *inBytesLeft=inLen;
          *outBuf=pOut;
          *inBuf=pIn;
          *numSub+=numS;
          return -1;
        }
      }
    }
    *outBytesLeft-=(pOut-*outBuf);
    *inBytesLeft=inLen;
    *outBuf=pOut;
    *inBuf=pIn;
    *numSub+=numS;
    return 0;
  } else if (cd->cnv_dmap->codingSchema == DMAP_U28) {
    /* See http://unicode.org/versions/corrigendum1.html */
    register int        inLen=*inBytesLeft;
    register char *     pOut=*outBuf;
    register char *     pIn=*inBuf;
    register char *     pLastOutBuf = *outBuf + *outBytesLeft - 1;
    //    register size_t     numS=0;
    while (0 < inLen) {
      register uint32_t       in;
      if (inLen == 1) {
        errno=EINVAL;   /* 22 */
        *inBytesLeft=inLen;
        *outBytesLeft-=(pOut-*outBuf);
        *outBuf=pOut;
        *inBuf=pIn;
        return -1;
      }
      if (pLastOutBuf < pOut)
        break;
      in=pIn[0];
      in<<=8;
      in+=pIn[1];
      if (in == 0x0000) {
        *pOut=0x00;
        ++pOut;
      } else if ((in & 0xFF80) == 0x0000) {     /* U28: in & 0b1111111110000000 == 0x0000 */
        *pOut=in;
        ++pOut;
      } else if ((in & 0xF800) == 0x0000) {     /* in & 0b1111100000000000 == 0x0000 */
        register uchar        byte;
        in>>=6;
        in&=0x001F;   /* 0b0000000000011111 */
        in|=0x00C0;   /* 0b0000000011000000 */
        *pOut=in;
        ++pOut;
        byte=pIn[1];
        byte&=0x3F;   /* 0b00111111; */
        byte|=0x80;   /* 0b10000000; */
        *pOut=byte;
        ++pOut;
      } else {
        register uchar        byte;
        register uchar        work;
        byte=pIn[0];
        byte>>=4;
        byte|=0xE0;   /* 0b11100000; */
        *pOut=byte;
        ++pOut;

        byte=pIn[0];
        byte<<=2;
        work=pIn[1];
        work>>=6;
        byte|=work;
        byte&=0x3F;   /* 0b00111111; */
        byte|=0x80;   /* 0b10000000; */
        *pOut=byte;
        ++pOut;

        byte=pIn[1];
        byte&=0x3F;   /* 0b00111111; */
        byte|=0x80;   /* 0b10000000; */
        *pOut=byte;
        ++pOut;
      }
      pIn+=2;
      inLen-=2;
    }
    *outBytesLeft-=(pOut-*outBuf);
    *inBytesLeft=inLen;
    *outBuf=pOut;
    *inBuf=pIn;
    //    *numSub+=numS;
    return 0;

  } else if (cd->cnv_dmap->codingSchema == DMAP_T28) {   /* UTF-16_UTF-8 */
    /* See http://unicode.org/versions/corrigendum1.html */
    register int        inLen=*inBytesLeft;
    register char *     pOut=*outBuf;
    register char *     pIn=*inBuf;
    register char *     pLastOutBuf = *outBuf + *outBytesLeft - 1;
    //    register size_t     numS=0;
    while (0 < inLen) {
      register uint32_t       in;
      if (inLen == 1) {
        errno=EINVAL;   /* 22 */
        *inBytesLeft=0;
        *outBytesLeft-=(pOut-*outBuf);
        *outBuf=pOut;
        *inBuf=pIn;
        return 0;
      }
      if (pLastOutBuf < pOut)
        break;
      in=pIn[0];
      in<<=8;
      in+=pIn[1];
      if (in == 0x0000) {
        *pOut=0x00;
        ++pOut;
      } else if ((in & 0xFF80) == 0x0000) {     /* U28: in & 0b1111111110000000 == 0x0000 */
        *pOut=in;
        ++pOut;
      } else if ((in & 0xF800) == 0x0000) {     /* in & 0b1111100000000000 == 0x0000 */
        register uchar        byte;
        in>>=6;
        in&=0x001F;   /* 0b0000000000011111 */
        in|=0x00C0;   /* 0b0000000011000000 */
        *pOut=in;
        ++pOut;
        byte=pIn[1];
        byte&=0x3F;   /* 0b00111111; */
        byte|=0x80;   /* 0b10000000; */
        *pOut=byte;
        ++pOut;
      } else if ((in & 0xFC00) == 0xD800) {     /* in & 0b1111110000000000 == 0b1101100000000000, first surrogate character */
        if (0xDC00 <= in ) {
          errno=EINVAL;   /* 22 */
          *inBytesLeft=inLen-1;
          *outBytesLeft-=(pOut-*outBuf);
          *outBuf=pOut;
          *inBuf=pIn;
          return -1;
          
        } else if (inLen < 4) {
          errno=EINVAL;   /* 22 */
          *inBytesLeft=inLen-2;
          *outBytesLeft-=(pOut-*outBuf);
          *outBuf=pOut;
          *inBuf=pIn+2;
          return -1;
          
        } else if ((pIn[2] & 0xFC) != 0xDC) {     /* pIn[2] & 0b11111100 == 0b11011100, second surrogate character */
          errno=EINVAL;   /* 22 */
          *inBytesLeft=inLen-2;
          *outBytesLeft-=(pOut-*outBuf);
          *outBuf=pOut;
          *inBuf=pIn+2;
          return -1;
          
        } else {
          register uchar        byte;
          register uchar        work;
          in>>=6;
          in&=0x000F;   /* 0b0000000000001111 */
          byte=in;      /* wwww  */
          ++byte;       /* uuuuu */
          work=byte;    /* save uuuuu */
          byte>>=2;
          byte|=0xF0;   /* 0b11110000; */
          *pOut=byte;
          ++pOut;

          byte=work;
          byte&=0x03;   /* 0b00000011; */
          byte<<=4;
          byte|=0x80;   /* 0b10000000; */
          work=pIn[1];
          work&=0x3C;   /* 0b00111100; */
          work>>=2;
          byte|=work;
          *pOut=byte;
          ++pOut;

          byte=pIn[1];
          byte&=0x03;   /* 0b00000011; */
          byte<<=4;
          byte|=0x80;   /* 0b10000000; */
          work=pIn[2];
          work&=0x03;   /* 0b00000011; */
          work<<=2;
          byte|=work;
          work=pIn[3];
          work>>=6;
          byte|=work;
          *pOut=byte;
          ++pOut;

          byte=pIn[3];
          byte&=0x3F;   /* 0b00111111; */
          byte|=0x80;   /* 0b10000000; */
          *pOut=byte;
          ++pOut;
          pIn+=2;
          inLen-=2;
        }
      } else if ((in & 0xFC00) == 0xDC00) {     /* in & 0b11111100 == 0b11011100, second surrogate character */
          errno=EINVAL;   /* 22 */
          *inBytesLeft=inLen-1;
          *outBytesLeft-=(pOut-*outBuf);
          *outBuf=pOut;
          *inBuf=pIn;
          return -1;
          
      } else {
        register uchar        byte;
        register uchar        work;
        byte=pIn[0];
        byte>>=4;
        byte|=0xE0;   /* 0b11100000; */
        *pOut=byte;
        ++pOut;

        byte=pIn[0];
        byte<<=2;
        work=pIn[1];
        work>>=6;
        byte|=work;
        byte&=0x3F;   /* 0b00111111; */
        byte|=0x80;   /* 0b10000000; */
        *pOut=byte;
        ++pOut;

        byte=pIn[1];
        byte&=0x3F;   /* 0b00111111; */
        byte|=0x80;   /* 0b10000000; */
        *pOut=byte;
        ++pOut;
      }
      pIn+=2;
      inLen-=2;
    }
    *outBytesLeft-=(pOut-*outBuf);
    *inBytesLeft=inLen;
    *outBuf=pOut;
    *inBuf=pIn;
    //    *numSub+=numS;
    return 0;

  } else if (cd->cnv_dmap->codingSchema == DMAP_U2U) {  /* UTF-16_UCS-2 */
    register int        inLen=*inBytesLeft;
    register int        outLen=*outBytesLeft;
    if (inLen <= outLen) {
      memcpy(*outBuf, *inBuf, inLen);
      (*outBytesLeft)-=inLen;
      (*inBuf)+=inLen;
      (*outBuf)+=inLen;
      *inBytesLeft=0;
      return 0;
    }
    memcpy(*outBuf, *inBuf, outLen);
    (*outBytesLeft)=0;
    (*inBuf)+=outLen;
    (*outBuf)+=outLen;
    *inBytesLeft-=outLen;
    return (*inBytesLeft);
      
  } else {
    return -1;
  }
  return 0;
}


#ifdef DEBUG
inline  size_t	        myconv(myconv_t cd ,
                               char**   inBuf,
                               size_t*  inBytesLeft,
                               char**   outBuf,
                               size_t*  outBytesLeft,
                               size_t*  numSub)
{
  if (cd->converterType == CONVERTER_ICONV) {
    return myconv_iconv(cd,inBuf,inBytesLeft,outBuf,outBytesLeft,numSub);
  } else if (cd->converterType == CONVERTER_DMAP) {
    return myconv_dmap(cd,inBuf,inBytesLeft,outBuf,outBytesLeft,numSub);
  }
  return -1;
}

inline  char *  converterName(int32_t   type)
{
  if (type == CONVERTER_ICONV)
    return "iconv";
  else if (type == CONVERTER_DMAP)
    return "dmap";
  
  return "?????";
}
#else
#define myconv(a,b,c,d,e,f) \
(((a)->converterType == CONVERTER_ICONV)? myconv_iconv((a),(b),(c),(d),(e),(f)): (((a)->converterType == CONVERTER_DMAP)? myconv_dmap((a),(b),(c),(d),(e),(f)): -1))


#define converterName(a) \
(((a) == CONVERTER_ICONV)? "iconv": ((a) == CONVERTER_DMAP)? "dmap": "?????")
#endif

void initMyconv();
void cleanupMyconv();

#endif
