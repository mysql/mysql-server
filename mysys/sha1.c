/* Copyright (C) 2000 MySQL AB & MySQL Finland AB & TCX DataKonsult AB                                                              
                                                                                                                                    
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
 sha1.c
 Original Source from: http://www.faqs.org/rfcs/rfc3174.html
 
 Description:
       This file implements the Secure Hashing Algorithm 1 as
       defined in FIPS PUB 180-1 published April 17, 1995.
 
       The SHA-1, produces a 160-bit message digest for a given
       data stream.  It should take about 2**n steps to find a
       message with the same digest as a given message and
       2**(n/2) to find any two messages with the same digest,
       when n is the digest size in bits.  Therefore, this
       algorithm can serve as a means of providing a
       "fingerprint" for a message.
 
   Portability Issues:
       SHA-1 is defined in terms of 32-bit "words".  This code
       uses <stdint.h> (included via "sha1.h" to define 32 and 8
       bit unsigned integer types.  If your C compiler does not
       support 32 bit unsigned integers, this code is not
       appropriate.
 
   Caveats:
       SHA-1 is designed to work with messages less than 2^64 bits
       long.  Although SHA-1 allows a message digest to be generated
       for messages of any number of bits less than 2^64, this
       implementation only works with messages with a length that is
       a multiple of the size of an 8-bit character.
 
*/
 
/* 
  Modified by 2002 by Peter Zaitsev to 
  - fit to new prototypes according to MySQL standard
  - Some optimizations 
  - All checking is now done in debug only mode
  - More comments 
*/

#include "sha1.h"

/*
 Define the SHA1 circular left shift macro
*/
 
#define SHA1CircularShift(bits,word) \
                (((word) << (bits)) | ((word) >> (32-(bits))))

/* Local Function Prototyptes */
void SHA1PadMessage(SHA1_CONTEXT*);
void SHA1ProcessMessageBlock(SHA1_CONTEXT*);

/*
 sha1_reset
 
 Description:
       This function will initialize the SHA1Context in preparation
       for computing a new SHA1 message digest.
 
 Parameters:
       context: [in/out]
           The context to reset.
 
 Returns:
       sha Error Code.
 
*/
 

const uint32 sha_const_key[5]=
{
  0x67452301,
  0xEFCDAB89,
  0x98BADCFE,
  0x10325476,
  0xC3D2E1F0
};


int sha1_reset(SHA1_CONTEXT *context)
{

#ifndef DBUG_OFF 
  if (!context)
  {
    return SHA_NULL;
  }
#endif    

  context->Length                 = 0;
  context->Message_Block_Index    = 0;
  
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
 sha1_result
 
 Description:
       This function will return the 160-bit message digest into the
       Message_Digest array  provided by the caller.
       NOTE: The first octet of hash is stored in the 0th element,
             the last octet of hash in the 19th element.
 
 Parameters:
       context: [in/out]
           The context to use to calculate the SHA-1 hash.
       Message_Digest: [out]
           Where the digest is returned.
 
 Returns:
       sha Error Code.
 
*/

int sha1_result( SHA1_CONTEXT *context,
                uint8 Message_Digest[SHA1_HASH_SIZE])
{
  int i;
    
#ifndef DBUG_OFF 
  if (!context || !Message_Digest)
  {
    return SHA_NULL;
  }

  if (context->Corrupted)
  {
    return context->Corrupted;
  }

  if (!context->Computed)
  {

#endif     
    SHA1PadMessage(context);
    for (i=0; i<64; i++)
    {
      /* message may be sensitive, clear it out */
      context->Message_Block[i] = 0;
    }
    context->Length   = 0;    /* and clear length  */
    context->Computed = 1;

#ifndef DBUG_OFF
  }
#endif

  for (i = 0; i < SHA1_HASH_SIZE; i++)
  {
    Message_Digest[i] = context->Intermediate_Hash[i>>2]
                            >> 8 * ( 3 - ( i & 0x03 ) );
  }
  
  return SHA_SUCCESS;
}

/*
 sha1_input
 
 Description:
       This function accepts an array of octets as the next portion
       of the message.
 
   Parameters:
       context: [in/out]
           The SHA context to update
       message_array: [in]
           An array of characters representing the next portion of
           the message.
       length: [in]
           The length of the message in message_array
 
   Returns:
       sha Error Code.
 
*/

int sha1_input(SHA1_CONTEXT *context, const uint8 *message_array,
               unsigned length)
{
  if (!length)
  {
    return SHA_SUCCESS;
  }
#ifndef DBUG_OFF 
  /* We assume client konows what it is doing in non-debug mode */
  if (!context || !message_array)
  {
    return SHA_NULL;
  }
  if (context->Computed)
  {
    context->Corrupted = SHA_STATE_ERROR;
    return SHA_STATE_ERROR;
  }

  if (context->Corrupted)
  {
    return context->Corrupted;
  }    
  while (length-- && !context->Corrupted)
  
#else
  while (length--)
#endif     

  {
    context->Message_Block[context->Message_Block_Index++] =
                    (*message_array & 0xFF);
    context->Length  += 8;  /* Length is in bits */ 

#ifndef DBUG_OFF    
    /* 
     Then we're not debugging we assume we never will get message longer
     2^64 bits.
    */
    if (context->Length == 0)
    {
      /* Message is too long */
      context->Corrupted = 1;
    }
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
   SHA1ProcessMessageBlock
 
   Description:
       This function will process the next 512 bits of the message
       stored in the Message_Block array.
 
   Parameters:
       None.
 
   Returns:
       Nothing.
 
   Comments:

       Many of the variable names in this code, especially the
       single character names, were used because those were the
       names used in the publication.
 
 
*/

static const uint32  K[]=   
{ /* Constants defined in SHA-1   */
  0x5A827999,
  0x6ED9EBA1,
  0x8F1BBCDC,
  0xCA62C1D6
};


void SHA1ProcessMessageBlock(SHA1_CONTEXT *context)
{
  int           t;                 /* Loop counter                */
  uint32        temp;              /* Temporary word value        */
  uint32        W[80];             /* Word sequence               */
  uint32        A, B, C, D, E;     /* Word buffers                */
  int index;
    
  /*
   Initialize the first 16 words in the array W
   */

  for (t = 0; t < 16; t++)
  {
    index=t*4;
    W[t] = context->Message_Block[index] << 24;
    W[t] |= context->Message_Block[index + 1] << 16;
    W[t] |= context->Message_Block[index + 2] << 8;
    W[t] |= context->Message_Block[index + 3];
  }


  for(t = 16; t < 80; t++)
  {
    W[t] = SHA1CircularShift(1,W[t-3] ^ W[t-8] ^ W[t-14] ^ W[t-16]);
  }

  A = context->Intermediate_Hash[0];
  B = context->Intermediate_Hash[1];
  C = context->Intermediate_Hash[2];
  D = context->Intermediate_Hash[3];
  E = context->Intermediate_Hash[4];

  for(t = 0; t < 20; t++)
  {
    temp =  SHA1CircularShift(5,A) +
            ((B & C) | ((~B) & D)) + E + W[t] + K[0];
    E = D;
    D = C;
    C = SHA1CircularShift(30,B);
    B = A;
    A = temp;
  }
  
  for(t = 20; t < 40; t++)
  {
    temp = SHA1CircularShift(5,A) + (B ^ C ^ D) + E + W[t] + K[1];
    E = D;
    D = C;
    C = SHA1CircularShift(30,B);
    B = A;
    A = temp;
  }

  for(t = 40; t < 60; t++)
  {
    temp = SHA1CircularShift(5,A) +
           ((B & C) | (B & D) | (C & D)) + E + W[t] + K[2];
    E = D;
    D = C;
    C = SHA1CircularShift(30,B);
    B = A;
    A = temp;
  }

  for(t = 60; t < 80; t++)
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
   SHA1PadMessage
 

   Description:
       According to the standard, the message must be padded to an even
       512 bits.  The first padding bit must be a '1'.  The last 64
       bits represent the length of the original message.  All bits in
       between should be 0.  This function will pad the message
       according to those rules by filling the Message_Block array
       accordingly.  It will also call the ProcessMessageBlock function
       provided appropriately.  When it returns, it can be assumed that
       the message digest has been computed.
 
   Parameters:
       context: [in/out]
           The context to pad
       ProcessMessageBlock: [in]
           The appropriate SHA*ProcessMessageBlock function
   Returns:
       Nothing.
 
*/

void SHA1PadMessage(SHA1_CONTEXT *context)
{
  /*
   Check to see if the current message block is too small to hold
   the initial padding bits and length.  If so, we will pad the
   block, process it, and then continue padding into a second
   block.
  */
  
#ifdef SHA_OLD_CODE
  
  if (context->Message_Block_Index > 55)
  {
    context->Message_Block[context->Message_Block_Index++] = 0x80;
    while (context->Message_Block_Index < 64)
    {
      context->Message_Block[context->Message_Block_Index++] = 0;
    }

    SHA1ProcessMessageBlock(context);

    while (context->Message_Block_Index < 56)
    {
      context->Message_Block[context->Message_Block_Index++] = 0;
    }
  }
  else
  {
    context->Message_Block[context->Message_Block_Index++] = 0x80;
    while (context->Message_Block_Index < 56)
    {
      context->Message_Block[context->Message_Block_Index++] = 0;
    }
  }
  
#else
  int i=context->Message_Block_Index;

  if (i > 55)
  {
    context->Message_Block[i++] = 0x80;
    bzero((char*) &context->Message_Block[i],
          sizeof(context->Message_Block[0])*(64-i));
    context->Message_Block_Index=64;
    
    SHA1ProcessMessageBlock(context);
    
    /* This function sets context->Message_Block_Index to zero  */
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
            
#endif  

  /*
   Store the message length as the last 8 octets
  */
  
  context->Message_Block[56] = context->Length >> 56;
  context->Message_Block[57] = context->Length >> 48;
  context->Message_Block[58] = context->Length >> 40;
  context->Message_Block[59] = context->Length >> 32;
  context->Message_Block[60] = context->Length >> 24;
  context->Message_Block[61] = context->Length >> 16;
  context->Message_Block[62] = context->Length >> 8;
  context->Message_Block[63] = context->Length;

  SHA1ProcessMessageBlock(context);
}

