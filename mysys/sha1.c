/* Copyright (c) 2002, 2004, 2006 MySQL AB

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

 DESCRIPTION
  This file implements the Secure Hashing Algorithm 1 as
  defined in FIPS PUB 180-1 published April 17, 1995.

  The SHA-1, produces a 160-bit message digest for a given data
  stream.  It should take about 2**n steps to find a message with the
  same digest as a given message and 2**(n/2) to find any two
  messages with the same digest, when n is the digest size in bits.
  Therefore, this algorithm can serve as a means of providing a
  "fingerprint" for a message.

 PORTABILITY ISSUES
   SHA-1 is defined in terms of 32-bit "words".  This code uses
   <stdint.h> (included via "sha1.h" to define 32 and 8 bit unsigned
   integer types.  If your C compiler does not support 32 bit unsigned
   integers, this code is not appropriate.

 CAVEATS
   SHA-1 is designed to work with messages less than 2^64 bits long.
   Although SHA-1 allows a message digest to be generated for messages
   of any number of bits less than 2^64, this implementation only
   works with messages with a length that is a multiple of the size of
   an 8-bit character.

  CHANGES
    2002 by Peter Zaitsev to
     - fit to new prototypes according to MySQL standard
     - Some optimizations
     - All checking is now done in debug only mode
     - More comments
*/

#include "my_global.h"
#include "m_string.h"
#include "sha1.h"

/*
  Define the SHA1 circular left shift macro
*/

#define SHA1CircularShift(bits,word) \
		(((word) << (bits)) | ((word) >> (32-(bits))))

/* Local Function Prototyptes */
static void SHA1PadMessage(SHA1_CONTEXT*);
static void SHA1ProcessMessageBlock(SHA1_CONTEXT*);


/*
  Initialize SHA1Context

  SYNOPSIS
    mysql_sha1_reset()
    context [in/out]		The context to reset.

 DESCRIPTION
   This function will initialize the SHA1Context in preparation
   for computing a new SHA1 message digest.

 RETURN
   SHA_SUCCESS		ok
   != SHA_SUCCESS	sha Error Code.
*/


const uint32 sha_const_key[5]=
{
  0x67452301,
  0xEFCDAB89,
  0x98BADCFE,
  0x10325476,
  0xC3D2E1F0
};


int mysql_sha1_reset(SHA1_CONTEXT *context)
{
#ifndef DBUG_OFF
  if (!context)
    return SHA_NULL;
#endif

  context->Length		  = 0;
  context->Message_Block_Index	  = 0;

  context->Intermediate_Hash[0]   = sha_const_key[0];
  context->Intermediate_Hash[1]   = sha_const_key[1];
  context->Intermediate_Hash[2]   = sha_const_key[2];
  context->Intermediate_Hash[3]   = sha_const_key[3];
  context->Intermediate_Hash[4]   = sha_const_key[4];

  context->Computed   = 0;
  context->Corrupted  = 0;

  return SHA_SUCCESS;
}


/*
   Return the 160-bit message digest into the array provided by the caller

  SYNOPSIS
    mysql_sha1_result()
    context [in/out]		The context to use to calculate the SHA-1 hash.
    Message_Digest: [out]	Where the digest is returned.

  DESCRIPTION
    NOTE: The first octet of hash is stored in the 0th element,
	  the last octet of hash in the 19th element.

 RETURN
   SHA_SUCCESS		ok
   != SHA_SUCCESS	sha Error Code.
*/

int mysql_sha1_result(SHA1_CONTEXT *context,
                      uint8 Message_Digest[SHA1_HASH_SIZE])
{
  int i;

#ifndef DBUG_OFF
  if (!context || !Message_Digest)
    return SHA_NULL;

  if (context->Corrupted)
    return context->Corrupted;
#endif

  if (!context->Computed)
  {
    SHA1PadMessage(context);
     /* message may be sensitive, clear it out */
    bzero((char*) context->Message_Block,64);
    context->Length   = 0;    /* and clear length  */
    context->Computed = 1;
  }

  for (i = 0; i < SHA1_HASH_SIZE; i++)
    Message_Digest[i] = (int8)((context->Intermediate_Hash[i>>2] >> 8
			 * ( 3 - ( i & 0x03 ) )));
  return SHA_SUCCESS;
}


/*
  Accepts an array of octets as the next portion of the message.

  SYNOPSIS
   mysql_sha1_input()
   context [in/out]	The SHA context to update
   message_array	An array of characters representing the next portion
			of the message.
  length		The length of the message in message_array

 RETURN
   SHA_SUCCESS		ok
   != SHA_SUCCESS	sha Error Code.
*/

int mysql_sha1_input(SHA1_CONTEXT *context, const uint8 *message_array,
                     unsigned length)
{
  if (!length)
    return SHA_SUCCESS;

#ifndef DBUG_OFF
  /* We assume client konows what it is doing in non-debug mode */
  if (!context || !message_array)
    return SHA_NULL;
  if (context->Computed)
    return (context->Corrupted= SHA_STATE_ERROR);
  if (context->Corrupted)
    return context->Corrupted;
#endif

  while (length--)
  {
    context->Message_Block[context->Message_Block_Index++]=
      (*message_array & 0xFF);
    context->Length  += 8;  /* Length is in bits */

#ifndef DBUG_OFF
    /*
      Then we're not debugging we assume we never will get message longer
      2^64 bits.
    */
    if (context->Length == 0)
      return (context->Corrupted= 1);	   /* Message is too long */
#endif

    if (context->Message_Block_Index == 64)
    {
      SHA1ProcessMessageBlock(context);
    }
    message_array++;
  }
  return SHA_SUCCESS;
}


/*
  Process the next 512 bits of the message stored in the Message_Block array.

  SYNOPSIS
    SHA1ProcessMessageBlock()

   DESCRIPTION
     Many of the variable names in this code, especially the single
     character names, were used because those were the names used in
     the publication.
*/

/* Constants defined in SHA-1	*/
static const uint32  K[]=
{
  0x5A827999,
  0x6ED9EBA1,
  0x8F1BBCDC,
  0xCA62C1D6
};


static void SHA1ProcessMessageBlock(SHA1_CONTEXT *context)
{
  int		t;		   /* Loop counter		  */
  uint32	temp;		   /* Temporary word value	  */
  uint32	W[80];		   /* Word sequence		  */
  uint32	A, B, C, D, E;	   /* Word buffers		  */
  int idx;

  /*
    Initialize the first 16 words in the array W
  */

  for (t = 0; t < 16; t++)
  {
    idx=t*4;
    W[t] = context->Message_Block[idx] << 24;
    W[t] |= context->Message_Block[idx + 1] << 16;
    W[t] |= context->Message_Block[idx + 2] << 8;
    W[t] |= context->Message_Block[idx + 3];
  }


  for (t = 16; t < 80; t++)
  {
    W[t] = SHA1CircularShift(1,W[t-3] ^ W[t-8] ^ W[t-14] ^ W[t-16]);
  }

  A = context->Intermediate_Hash[0];
  B = context->Intermediate_Hash[1];
  C = context->Intermediate_Hash[2];
  D = context->Intermediate_Hash[3];
  E = context->Intermediate_Hash[4];

  for (t = 0; t < 20; t++)
  {
    temp= SHA1CircularShift(5,A) + ((B & C) | ((~B) & D)) + E + W[t] + K[0];
    E = D;
    D = C;
    C = SHA1CircularShift(30,B);
    B = A;
    A = temp;
  }

  for (t = 20; t < 40; t++)
  {
    temp = SHA1CircularShift(5,A) + (B ^ C ^ D) + E + W[t] + K[1];
    E = D;
    D = C;
    C = SHA1CircularShift(30,B);
    B = A;
    A = temp;
  }

  for (t = 40; t < 60; t++)
  {
    temp= (SHA1CircularShift(5,A) + ((B & C) | (B & D) | (C & D)) + E + W[t] +
	   K[2]);
    E = D;
    D = C;
    C = SHA1CircularShift(30,B);
    B = A;
    A = temp;
  }

  for (t = 60; t < 80; t++)
  {
    temp = SHA1CircularShift(5,A) + (B ^ C ^ D) + E + W[t] + K[3];
    E = D;
    D = C;
    C = SHA1CircularShift(30,B);
    B = A;
    A = temp;
  }

  context->Intermediate_Hash[0] += A;
  context->Intermediate_Hash[1] += B;
  context->Intermediate_Hash[2] += C;
  context->Intermediate_Hash[3] += D;
  context->Intermediate_Hash[4] += E;

  context->Message_Block_Index = 0;
}


/*
  Pad message

  SYNOPSIS
    SHA1PadMessage()
    context: [in/out]		The context to pad

  DESCRIPTION
    According to the standard, the message must be padded to an even
    512 bits.  The first padding bit must be a '1'. The last 64 bits
    represent the length of the original message.  All bits in between
    should be 0.  This function will pad the message according to
    those rules by filling the Message_Block array accordingly.  It
    will also call the ProcessMessageBlock function provided
    appropriately. When it returns, it can be assumed that the message
    digest has been computed.

*/

static void SHA1PadMessage(SHA1_CONTEXT *context)
{
  /*
    Check to see if the current message block is too small to hold
    the initial padding bits and length.  If so, we will pad the
    block, process it, and then continue padding into a second
    block.
  */

  int i=context->Message_Block_Index;

  if (i > 55)
  {
    context->Message_Block[i++] = 0x80;
    bzero((char*) &context->Message_Block[i],
	  sizeof(context->Message_Block[0])*(64-i));
    context->Message_Block_Index=64;

    /* This function sets context->Message_Block_Index to zero	*/
    SHA1ProcessMessageBlock(context);

    bzero((char*) &context->Message_Block[0],
	  sizeof(context->Message_Block[0])*56);
    context->Message_Block_Index=56;
  }
  else
  {
    context->Message_Block[i++] = 0x80;
    bzero((char*) &context->Message_Block[i],
	  sizeof(context->Message_Block[0])*(56-i));
    context->Message_Block_Index=56;
  }

  /*
    Store the message length as the last 8 octets
  */

  context->Message_Block[56] = (int8) (context->Length >> 56);
  context->Message_Block[57] = (int8) (context->Length >> 48);
  context->Message_Block[58] = (int8) (context->Length >> 40);
  context->Message_Block[59] = (int8) (context->Length >> 32);
  context->Message_Block[60] = (int8) (context->Length >> 24);
  context->Message_Block[61] = (int8) (context->Length >> 16);
  context->Message_Block[62] = (int8) (context->Length >> 8);
  context->Message_Block[63] = (int8) (context->Length);

  SHA1ProcessMessageBlock(context);
}
