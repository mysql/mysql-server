#ifndef SHA1_INCLUDED
#define SHA1_INCLUDED

/* Copyright (C) 2002, 2006 MySQL AB

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; version 2 of the License.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
*/

/*
 This is the header file for code which implements the Secure
 Hashing Algorithm 1 as defined in FIPS PUB 180-1 published
 April 17, 1995.

 Many of the variable names in this code, especially the
 single character names, were used because those were the names
 used in the publication.

 Please read the file sha1.c for more information.

 Modified 2002 by Peter Zaitsev to better follow MySQL standards

  Original Source from: http://www.faqs.org/rfcs/rfc3174.html

  Copyright (C) The Internet Society (2001).  All Rights Reserved.

  This document and translations of it may be copied and furnished to
  others, and derivative works that comment on or otherwise explain it
  or assist in its implementation may be prepared, copied, published
  and distributed, in whole or in part, without restriction of any
  kind, provided that the above copyright notice and this paragraph are
  included on all such copies and derivative works.  However, this
  document itself may not be modified in any way, such as by removing
  the copyright notice or references to the Internet Society or other
  Internet organizations, except as needed for the purpose of
  developing Internet standards in which case the procedures for
  copyrights defined in the Internet Standards process must be
  followed, or as required to translate it into languages other than
  English.

  The limited permissions granted above are perpetual and will not be
  revoked by the Internet Society or its successors or assigns.

  This document and the information contained herein is provided on an
  "AS IS" basis and THE INTERNET SOCIETY AND THE INTERNET ENGINEERING
  TASK FORCE DISCLAIMS ALL WARRANTIES, EXPRESS OR IMPLIED, INCLUDING
  BUT NOT LIMITED TO ANY WARRANTY THAT THE USE OF THE INFORMATION
  HEREIN WILL NOT INFRINGE ANY RIGHTS OR ANY IMPLIED WARRANTIES OF
  MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE.

  Acknowledgement 
  Funding for the RFC Editor function is currently provided by the 
  Internet Society. 
*/


enum sha_result_codes
{
  SHA_SUCCESS = 0,
  SHA_NULL,		/* Null pointer parameter */
  SHA_INPUT_TOO_LONG,	/* input data too long */
  SHA_STATE_ERROR	/* called Input after Result */
};

#define SHA1_HASH_SIZE 20 /* Hash size in bytes */

/*
  This structure will hold context information for the SHA-1
  hashing operation
*/

typedef struct SHA1_CONTEXT
{
  ulonglong  Length;		/* Message length in bits      */
  uint32 Intermediate_Hash[SHA1_HASH_SIZE/4]; /* Message Digest  */
  int Computed;			/* Is the digest computed?	   */
  int Corrupted;		/* Is the message digest corrupted? */
  int16 Message_Block_Index;	/* Index into message block array   */
  uint8 Message_Block[64];	/* 512-bit message blocks      */
} SHA1_CONTEXT;

/*
  Function Prototypes
*/

C_MODE_START

int mysql_sha1_reset(SHA1_CONTEXT*);
int mysql_sha1_input(SHA1_CONTEXT*, const uint8 *, unsigned int);
int mysql_sha1_result(SHA1_CONTEXT* , uint8 Message_Digest[SHA1_HASH_SIZE]);

C_MODE_END

#endif /* SHA__INCLUDED */
