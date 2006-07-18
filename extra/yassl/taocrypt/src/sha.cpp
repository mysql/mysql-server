/* sha.cpp                                
 *
 * Copyright (C) 2003 Sawtooth Consulting Ltd.
 *
 * This file is part of yaSSL.
 *
 * yaSSL is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * There are special exceptions to the terms and conditions of the GPL as it
 * is applied to yaSSL. View the full text of the exception in the file
 * FLOSS-EXCEPTIONS in the directory of this software distribution.
 *
 * yaSSL is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA
 */

/* based on Wei Dai's sha.cpp from CryptoPP */

#include "runtime.hpp"
#include <string.h>
#include "algorithm.hpp"    // mySTL::swap
#include "sha.hpp"


#if defined(TAOCRYPT_X86ASM_AVAILABLE) && defined(TAO_ASM)
    #define DO_SHA_ASM
#endif


namespace TaoCrypt {

#define blk0(i) (W[i] = buffer_[i])
#define blk1(i) (W[i&15] = \
                 rotlFixed(W[(i+13)&15]^W[(i+8)&15]^W[(i+2)&15]^W[i&15],1))

#define f1(x,y,z) (z^(x &(y^z)))
#define f2(x,y,z) (x^y^z)
#define f3(x,y,z) ((x&y)|(z&(x|y)))
#define f4(x,y,z) (x^y^z)

// (R0+R1), R2, R3, R4 are the different operations used in SHA1
#define R0(v,w,x,y,z,i) z+= f1(w,x,y) + blk0(i) + 0x5A827999+ \
                        rotlFixed(v,5); w = rotlFixed(w,30);
#define R1(v,w,x,y,z,i) z+= f1(w,x,y) + blk1(i) + 0x5A827999+ \
                        rotlFixed(v,5); w = rotlFixed(w,30);
#define R2(v,w,x,y,z,i) z+= f2(w,x,y) + blk1(i) + 0x6ED9EBA1+ \
                        rotlFixed(v,5); w = rotlFixed(w,30);
#define R3(v,w,x,y,z,i) z+= f3(w,x,y) + blk1(i) + 0x8F1BBCDC+ \
                        rotlFixed(v,5); w = rotlFixed(w,30);
#define R4(v,w,x,y,z,i) z+= f4(w,x,y) + blk1(i) + 0xCA62C1D6+ \
                        rotlFixed(v,5); w = rotlFixed(w,30);


void SHA::Init()
{
    digest_[0] = 0x67452301L;
    digest_[1] = 0xEFCDAB89L;
    digest_[2] = 0x98BADCFEL;
    digest_[3] = 0x10325476L;
    digest_[4] = 0xC3D2E1F0L;

    buffLen_ = 0;
    loLen_  = 0;
    hiLen_  = 0;
}


SHA::SHA(const SHA& that) : HASHwithTransform(DIGEST_SIZE / sizeof(word32),
                                              BLOCK_SIZE) 
{ 
    buffLen_ = that.buffLen_;
    loLen_   = that.loLen_;
    hiLen_   = that.hiLen_;

    memcpy(digest_, that.digest_, DIGEST_SIZE);
    memcpy(buffer_, that.buffer_, BLOCK_SIZE);
}

SHA& SHA::operator= (const SHA& that)
{
    SHA tmp(that);
    Swap(tmp);

    return *this;
}


void SHA::Swap(SHA& other)
{
    mySTL::swap(loLen_,   other.loLen_);
    mySTL::swap(hiLen_,   other.hiLen_);
    mySTL::swap(buffLen_, other.buffLen_);

    memcpy(digest_, other.digest_, DIGEST_SIZE);
    memcpy(buffer_, other.buffer_, BLOCK_SIZE);
}


// Update digest with data of size len, do in blocks
void SHA::Update(const byte* data, word32 len)
{
    byte* local = (byte*)buffer_;

    // remove buffered data if possible
    if (buffLen_)  {   
        word32 add = min(len, BLOCK_SIZE - buffLen_);
        memcpy(&local[buffLen_], data, add);

        buffLen_ += add;
        data     += add;
        len      -= add;

        if (buffLen_ == BLOCK_SIZE) {
            ByteReverseIf(local, local, BLOCK_SIZE, BigEndianOrder);
            Transform();
            AddLength(BLOCK_SIZE);
            buffLen_ = 0;
        }
    }

    // do block size transforms or all at once for asm
    if (buffLen_ == 0) {
        #ifndef DO_SHA_ASM
            while (len >= BLOCK_SIZE) {
                memcpy(&local[0], data, BLOCK_SIZE);

                data     += BLOCK_SIZE;
                len      -= BLOCK_SIZE;

                ByteReverseIf(local, local, BLOCK_SIZE, BigEndianOrder);
                Transform();
                AddLength(BLOCK_SIZE);
            }
        #else
            word32 times = len / BLOCK_SIZE;
            if (times) {
                AsmTransform(data, times);
                const word32 add = BLOCK_SIZE * times;
                AddLength(add);
                 len  -= add;
                data += add;
            }
        #endif
    }

    // cache any data left
    if (len) {
        memcpy(&local[buffLen_], data, len);
        buffLen_ += len;
    }
}


void SHA::Transform()
{
    word32 W[BLOCK_SIZE / sizeof(word32)];

    // Copy context->state[] to working vars 
    word32 a = digest_[0];
    word32 b = digest_[1];
    word32 c = digest_[2];
    word32 d = digest_[3];
    word32 e = digest_[4];

    // 4 rounds of 20 operations each. Loop unrolled. 
    R0(a,b,c,d,e, 0); R0(e,a,b,c,d, 1); R0(d,e,a,b,c, 2); R0(c,d,e,a,b, 3);
    R0(b,c,d,e,a, 4); R0(a,b,c,d,e, 5); R0(e,a,b,c,d, 6); R0(d,e,a,b,c, 7);
    R0(c,d,e,a,b, 8); R0(b,c,d,e,a, 9); R0(a,b,c,d,e,10); R0(e,a,b,c,d,11);
    R0(d,e,a,b,c,12); R0(c,d,e,a,b,13); R0(b,c,d,e,a,14); R0(a,b,c,d,e,15);

    R1(e,a,b,c,d,16); R1(d,e,a,b,c,17); R1(c,d,e,a,b,18); R1(b,c,d,e,a,19);

    R2(a,b,c,d,e,20); R2(e,a,b,c,d,21); R2(d,e,a,b,c,22); R2(c,d,e,a,b,23);
    R2(b,c,d,e,a,24); R2(a,b,c,d,e,25); R2(e,a,b,c,d,26); R2(d,e,a,b,c,27);
    R2(c,d,e,a,b,28); R2(b,c,d,e,a,29); R2(a,b,c,d,e,30); R2(e,a,b,c,d,31);
    R2(d,e,a,b,c,32); R2(c,d,e,a,b,33); R2(b,c,d,e,a,34); R2(a,b,c,d,e,35);
    R2(e,a,b,c,d,36); R2(d,e,a,b,c,37); R2(c,d,e,a,b,38); R2(b,c,d,e,a,39);

    R3(a,b,c,d,e,40); R3(e,a,b,c,d,41); R3(d,e,a,b,c,42); R3(c,d,e,a,b,43);
    R3(b,c,d,e,a,44); R3(a,b,c,d,e,45); R3(e,a,b,c,d,46); R3(d,e,a,b,c,47);
    R3(c,d,e,a,b,48); R3(b,c,d,e,a,49); R3(a,b,c,d,e,50); R3(e,a,b,c,d,51);
    R3(d,e,a,b,c,52); R3(c,d,e,a,b,53); R3(b,c,d,e,a,54); R3(a,b,c,d,e,55);
    R3(e,a,b,c,d,56); R3(d,e,a,b,c,57); R3(c,d,e,a,b,58); R3(b,c,d,e,a,59);

    R4(a,b,c,d,e,60); R4(e,a,b,c,d,61); R4(d,e,a,b,c,62); R4(c,d,e,a,b,63);
    R4(b,c,d,e,a,64); R4(a,b,c,d,e,65); R4(e,a,b,c,d,66); R4(d,e,a,b,c,67);
    R4(c,d,e,a,b,68); R4(b,c,d,e,a,69); R4(a,b,c,d,e,70); R4(e,a,b,c,d,71);
    R4(d,e,a,b,c,72); R4(c,d,e,a,b,73); R4(b,c,d,e,a,74); R4(a,b,c,d,e,75);
    R4(e,a,b,c,d,76); R4(d,e,a,b,c,77); R4(c,d,e,a,b,78); R4(b,c,d,e,a,79);

    // Add the working vars back into digest state[]
    digest_[0] += a;
    digest_[1] += b;
    digest_[2] += c;
    digest_[3] += d;
    digest_[4] += e;

    // Wipe variables
    a = b = c = d = e = 0;
    memset(W, 0, sizeof(W));
}


#ifdef DO_SHA_ASM

// f1(x,y,z) (z^(x &(y^z)))
// place in esi
#define ASMf1(x,y,z)   \
    AS2(    mov   esi, y    )   \
    AS2(    xor   esi, z    )   \
    AS2(    and   esi, x    )   \
    AS2(    xor   esi, z    )


// R0(v,w,x,y,z,i) =
//      z+= f1(w,x,y) + W[i] + 0x5A827999 + rotlFixed(v,5);
//      w = rotlFixed(w,30);

//      use esi for f
//      use edi as tmp


#define ASMR0(v,w,x,y,z,i) \
    AS2(    mov   esi, x                        )   \
    AS2(    mov   edi, [esp + i * 4]            )   \
    AS2(    xor   esi, y                        )   \
    AS2(    and   esi, w                        )   \
    AS2(    lea     z, [edi + z + 0x5A827999]   )   \
    AS2(    mov   edi, v                        )   \
    AS2(    xor   esi, y                        )   \
    AS2(    rol   edi, 5                        )   \
    AS2(    add     z, esi                      )   \
    AS2(    rol     w, 30                       )   \
    AS2(    add     z, edi                      )


/*  Some macro stuff, but older gas ( < 2,16 ) can't process &, so do by hand
    % won't work on gas at all

#define xstr(s) str(s)
#define  str(s) #s

#define WOFF1(a) ( a       & 15)
#define WOFF2(a) ((a +  2) & 15)
#define WOFF3(a) ((a +  8) & 15)
#define WOFF4(a) ((a + 13) & 15)

#ifdef __GNUC__
    #define WGET1(i) asm("mov esp, [edi - "xstr(WOFF1(i))" * 4] ");
    #define WGET2(i) asm("xor esp, [edi - "xstr(WOFF2(i))" * 4] ");
    #define WGET3(i) asm("xor esp, [edi - "xstr(WOFF3(i))" * 4] ");
    #define WGET4(i) asm("xor esp, [edi - "xstr(WOFF4(i))" * 4] ");
    #define WPUT1(i) asm("mov [edi - "xstr(WOFF1(i))" * 4], esp ");
#else
    #define WGET1(i) AS2( mov   esp, [edi - WOFF1(i) * 4]   )
    #define WGET2(i) AS2( xor   esp, [edi - WOFF2(i) * 4]   )
    #define WGET3(i) AS2( xor   esp, [edi - WOFF3(i) * 4]   )
    #define WGET4(i) AS2( xor   esp, [edi - WOFF4(i) * 4]   )
    #define WPUT1(i) AS2( mov   [edi - WOFF1(i) * 4], esp   )
#endif
*/

// ASMR1 = ASMR0 but use esp for W calcs

#define ASMR1(v,w,x,y,z,i,W1,W2,W3,W4) \
    AS2(    mov   edi, [esp + W1 * 4]           )   \
    AS2(    mov   esi, x                        )   \
    AS2(    xor   edi, [esp + W2 * 4]           )   \
    AS2(    xor   esi, y                        )   \
    AS2(    xor   edi, [esp + W3 * 4]           )   \
    AS2(    and   esi, w                        )   \
    AS2(    xor   edi, [esp + W4 * 4]           )   \
    AS2(    rol   edi, 1                        )   \
    AS2(    xor   esi, y                        )   \
    AS2(    mov   [esp + W1 * 4], edi           )   \
    AS2(    lea     z, [edi + z + 0x5A827999]   )   \
    AS2(    mov   edi, v                        )   \
    AS2(    rol   edi, 5                        )   \
    AS2(    add     z, esi                      )   \
    AS2(    rol     w, 30                       )   \
    AS2(    add     z, edi                      )


// ASMR2 = ASMR1 but f is xor, xor instead

#define ASMR2(v,w,x,y,z,i,W1,W2,W3,W4) \
    AS2(    mov   edi, [esp + W1 * 4]           )   \
    AS2(    mov   esi, x                        )   \
    AS2(    xor   edi, [esp + W2 * 4]           )   \
    AS2(    xor   esi, y                        )   \
    AS2(    xor   edi, [esp + W3 * 4]           )   \
    AS2(    xor   esi, w                        )   \
    AS2(    xor   edi, [esp + W4 * 4]           )   \
    AS2(    rol   edi, 1                        )   \
    AS2(    add     z, esi                      )   \
    AS2(    mov   [esp + W1 * 4], edi           )   \
    AS2(    lea     z, [edi + z + 0x6ED9EBA1]   )   \
    AS2(    mov   edi, v                        )   \
    AS2(    rol   edi, 5                        )   \
    AS2(    rol     w, 30                       )   \
    AS2(    add     z, edi                      )


// ASMR3 = ASMR2 but f is (x&y)|(z&(x|y))
//               which is (w&x)|(y&(w|x))

#define ASMR3(v,w,x,y,z,i,W1,W2,W3,W4) \
    AS2(    mov   edi, [esp + W1 * 4]           )   \
    AS2(    mov   esi, x                        )   \
    AS2(    xor   edi, [esp + W2 * 4]           )   \
    AS2(     or   esi, w                        )   \
    AS2(    xor   edi, [esp + W3 * 4]           )   \
    AS2(    and   esi, y                        )   \
    AS2(    xor   edi, [esp + W4 * 4]           )   \
    AS2(    movd  mm0, esi                      )   \
    AS2(    rol   edi, 1                        )   \
    AS2(    mov   esi, x                        )   \
    AS2(    mov   [esp + W1 * 4], edi           )   \
    AS2(    and   esi, w                        )   \
    AS2(    lea     z, [edi + z + 0x8F1BBCDC]   )   \
    AS2(    movd  edi, mm0                      )   \
    AS2(     or   esi, edi                      )   \
    AS2(    mov   edi, v                        )   \
    AS2(    rol   edi, 5                        )   \
    AS2(    add     z, esi                      )   \
    AS2(    rol     w, 30                       )   \
    AS2(    add     z, edi                      )


// ASMR4 = ASMR2 but different constant

#define ASMR4(v,w,x,y,z,i,W1,W2,W3,W4) \
    AS2(    mov   edi, [esp + W1 * 4]           )   \
    AS2(    mov   esi, x                        )   \
    AS2(    xor   edi, [esp + W2 * 4]           )   \
    AS2(    xor   esi, y                        )   \
    AS2(    xor   edi, [esp + W3 * 4]           )   \
    AS2(    xor   esi, w                        )   \
    AS2(    xor   edi, [esp + W4 * 4]           )   \
    AS2(    rol   edi, 1                        )   \
    AS2(    add     z, esi                      )   \
    AS2(    mov   [esp + W1 * 4], edi           )   \
    AS2(    lea     z, [edi + z + 0xCA62C1D6]   )   \
    AS2(    mov   edi, v                        )   \
    AS2(    rol   edi, 5                        )   \
    AS2(    rol     w, 30                       )   \
    AS2(    add     z, edi                      )


#ifdef _MSC_VER
    __declspec(naked) 
#endif
void SHA::AsmTransform(const byte* data, word32 times)
{
#ifdef __GNUC__
    #define AS1(x)    asm(#x);
    #define AS2(x, y) asm(#x ", " #y);

    #define PROLOG()  \
        asm(".intel_syntax noprefix"); \
        AS2(    movd  mm3, edi                      )   \
        AS2(    movd  mm4, ebx                      )   \
        AS2(    movd  mm5, esi                      )   \
        AS2(    movd  mm6, ebp                      )   \
        AS2(    mov   ecx, DWORD PTR [ebp +  8]     )   \
        AS2(    mov   edi, DWORD PTR [ebp + 12]     )   \
        AS2(    mov   eax, DWORD PTR [ebp + 16]     )

    #define EPILOG()  \
        AS2(    movd  ebp, mm6                  )   \
        AS2(    movd  esi, mm5                  )   \
        AS2(    movd  ebx, mm4                  )   \
        AS2(    mov   esp, ebp                  )   \
        AS2(    movd  edi, mm3                  )   \
        AS1(    emms                            )   \
        asm(".att_syntax");
#else
    #define AS1(x)    __asm x
    #define AS2(x, y) __asm x, y

    #define PROLOG() \
        AS1(    push  ebp                           )   \
        AS2(    mov   ebp, esp                      )   \
        AS2(    movd  mm3, edi                      )   \
        AS2(    movd  mm4, ebx                      )   \
        AS2(    movd  mm5, esi                      )   \
        AS2(    movd  mm6, ebp                      )   \
        AS2(    mov   edi, data                     )   \
        AS2(    mov   eax, times                    )

    #define EPILOG() \
        AS2(    movd  ebp, mm6                  )   \
        AS2(    movd  esi, mm5                  )   \
        AS2(    movd  ebx, mm4                  )   \
        AS2(    movd  edi, mm3                  )   \
        AS2(    mov   esp, ebp                  )   \
        AS1(    pop   ebp                       )   \
        AS1(    emms   )                            \
        AS1(    ret 8  )   
#endif

    PROLOG()

    AS2(    mov   esi, ecx              )

    #ifdef OLD_GCC_OFFSET
        AS2(    add   esi, 20               )   // digest_[0]
    #else
        AS2(    add   esi, 16               )   // digest_[0]
    #endif

    AS2(    movd  mm2, eax              )   // store times_
    AS2(    movd  mm1, esi              )   // store digest_

    AS2(    sub   esp, 68               )   // make room on stack

AS1( loopStart: )

    // byte reverse 16 words of input, 4 at a time, put on stack for W[]

    // part 1
    AS2(    mov   eax, [edi]        )
    AS2(    mov   ebx, [edi +  4]   )
    AS2(    mov   ecx, [edi +  8]   )
    AS2(    mov   edx, [edi + 12]   )

    AS1(    bswap eax   )
    AS1(    bswap ebx   )
    AS1(    bswap ecx   )
    AS1(    bswap edx   )

    AS2(    mov   [esp],      eax   )
    AS2(    mov   [esp +  4], ebx   )
    AS2(    mov   [esp +  8], ecx   )
    AS2(    mov   [esp + 12], edx   )

    // part 2
    AS2(    mov   eax, [edi + 16]   )
    AS2(    mov   ebx, [edi + 20]   )
    AS2(    mov   ecx, [edi + 24]   )
    AS2(    mov   edx, [edi + 28]   )

    AS1(    bswap eax   )
    AS1(    bswap ebx   )
    AS1(    bswap ecx   )
    AS1(    bswap edx   )

    AS2(    mov   [esp + 16], eax   )
    AS2(    mov   [esp + 20], ebx   )
    AS2(    mov   [esp + 24], ecx   )
    AS2(    mov   [esp + 28], edx   )


    // part 3
    AS2(    mov   eax, [edi + 32]   )
    AS2(    mov   ebx, [edi + 36]   )
    AS2(    mov   ecx, [edi + 40]   )
    AS2(    mov   edx, [edi + 44]   )

    AS1(    bswap eax   )
    AS1(    bswap ebx   )
    AS1(    bswap ecx   )
    AS1(    bswap edx   )

    AS2(    mov   [esp + 32], eax   )
    AS2(    mov   [esp + 36], ebx   )
    AS2(    mov   [esp + 40], ecx   )
    AS2(    mov   [esp + 44], edx   )


    // part 4
    AS2(    mov   eax, [edi + 48]   )
    AS2(    mov   ebx, [edi + 52]   )
    AS2(    mov   ecx, [edi + 56]   )
    AS2(    mov   edx, [edi + 60]   )

    AS1(    bswap eax   )
    AS1(    bswap ebx   )
    AS1(    bswap ecx   )
    AS1(    bswap edx   )

    AS2(    mov   [esp + 48], eax   )
    AS2(    mov   [esp + 52], ebx   )
    AS2(    mov   [esp + 56], ecx   )
    AS2(    mov   [esp + 60], edx   )

    AS2(    mov   [esp + 64], edi   )   // store edi for end

    // read from digest_
    AS2(    mov   eax, [esi]            )   // a1
    AS2(    mov   ebx, [esi +  4]       )   // b1
    AS2(    mov   ecx, [esi +  8]       )   // c1
    AS2(    mov   edx, [esi + 12]       )   // d1
    AS2(    mov   ebp, [esi + 16]       )   // e1


    ASMR0(eax, ebx, ecx, edx, ebp,  0)
    ASMR0(ebp, eax, ebx, ecx, edx,  1)
    ASMR0(edx, ebp, eax, ebx, ecx,  2)
    ASMR0(ecx, edx, ebp, eax, ebx,  3)
    ASMR0(ebx, ecx, edx, ebp, eax,  4)
    ASMR0(eax, ebx, ecx, edx, ebp,  5)
    ASMR0(ebp, eax, ebx, ecx, edx,  6)
    ASMR0(edx, ebp, eax, ebx, ecx,  7)
    ASMR0(ecx, edx, ebp, eax, ebx,  8)
    ASMR0(ebx, ecx, edx, ebp, eax,  9)
    ASMR0(eax, ebx, ecx, edx, ebp, 10)
    ASMR0(ebp, eax, ebx, ecx, edx, 11)
    ASMR0(edx, ebp, eax, ebx, ecx, 12)
    ASMR0(ecx, edx, ebp, eax, ebx, 13)
    ASMR0(ebx, ecx, edx, ebp, eax, 14)
    ASMR0(eax, ebx, ecx, edx, ebp, 15)

    ASMR1(ebp, eax, ebx, ecx, edx, 16,  0,  2,  8, 13)
    ASMR1(edx, ebp, eax, ebx, ecx, 17,  1,  3,  9, 14)
    ASMR1(ecx, edx, ebp, eax, ebx, 18,  2,  4, 10, 15)
    ASMR1(ebx, ecx, edx, ebp, eax, 19,  3,  5, 11,  0)

    ASMR2(eax, ebx, ecx, edx, ebp, 20,  4,  6, 12,  1)
    ASMR2(ebp, eax, ebx, ecx, edx, 21,  5,  7, 13,  2)
    ASMR2(edx, ebp, eax, ebx, ecx, 22,  6,  8, 14,  3)
    ASMR2(ecx, edx, ebp, eax, ebx, 23,  7,  9, 15,  4)
    ASMR2(ebx, ecx, edx, ebp, eax, 24,  8, 10,  0,  5)
    ASMR2(eax, ebx, ecx, edx, ebp, 25,  9, 11,  1,  6)
    ASMR2(ebp, eax, ebx, ecx, edx, 26, 10, 12,  2,  7)
    ASMR2(edx, ebp, eax, ebx, ecx, 27, 11, 13,  3,  8)
    ASMR2(ecx, edx, ebp, eax, ebx, 28, 12, 14,  4,  9)
    ASMR2(ebx, ecx, edx, ebp, eax, 29, 13, 15,  5, 10)
    ASMR2(eax, ebx, ecx, edx, ebp, 30, 14,  0,  6, 11)
    ASMR2(ebp, eax, ebx, ecx, edx, 31, 15,  1,  7, 12)
    ASMR2(edx, ebp, eax, ebx, ecx, 32,  0,  2,  8, 13)
    ASMR2(ecx, edx, ebp, eax, ebx, 33,  1,  3,  9, 14)
    ASMR2(ebx, ecx, edx, ebp, eax, 34,  2,  4, 10, 15)
    ASMR2(eax, ebx, ecx, edx, ebp, 35,  3,  5, 11,  0)
    ASMR2(ebp, eax, ebx, ecx, edx, 36,  4,  6, 12,  1)
    ASMR2(edx, ebp, eax, ebx, ecx, 37,  5,  7, 13,  2)
    ASMR2(ecx, edx, ebp, eax, ebx, 38,  6,  8, 14,  3)
    ASMR2(ebx, ecx, edx, ebp, eax, 39,  7,  9, 15,  4)


    ASMR3(eax, ebx, ecx, edx, ebp, 40,  8, 10,  0,  5)
    ASMR3(ebp, eax, ebx, ecx, edx, 41,  9, 11,  1,  6)
    ASMR3(edx, ebp, eax, ebx, ecx, 42, 10, 12,  2,  7)
    ASMR3(ecx, edx, ebp, eax, ebx, 43, 11, 13,  3,  8)
    ASMR3(ebx, ecx, edx, ebp, eax, 44, 12, 14,  4,  9)
    ASMR3(eax, ebx, ecx, edx, ebp, 45, 13, 15,  5, 10)
    ASMR3(ebp, eax, ebx, ecx, edx, 46, 14,  0,  6, 11)
    ASMR3(edx, ebp, eax, ebx, ecx, 47, 15,  1,  7, 12)
    ASMR3(ecx, edx, ebp, eax, ebx, 48,  0,  2,  8, 13)
    ASMR3(ebx, ecx, edx, ebp, eax, 49,  1,  3,  9, 14)
    ASMR3(eax, ebx, ecx, edx, ebp, 50,  2,  4, 10, 15)
    ASMR3(ebp, eax, ebx, ecx, edx, 51,  3,  5, 11,  0)
    ASMR3(edx, ebp, eax, ebx, ecx, 52,  4,  6, 12,  1)
    ASMR3(ecx, edx, ebp, eax, ebx, 53,  5,  7, 13,  2)
    ASMR3(ebx, ecx, edx, ebp, eax, 54,  6,  8, 14,  3)
    ASMR3(eax, ebx, ecx, edx, ebp, 55,  7,  9, 15,  4)
    ASMR3(ebp, eax, ebx, ecx, edx, 56,  8, 10,  0,  5)
    ASMR3(edx, ebp, eax, ebx, ecx, 57,  9, 11,  1,  6)
    ASMR3(ecx, edx, ebp, eax, ebx, 58, 10, 12,  2,  7)
    ASMR3(ebx, ecx, edx, ebp, eax, 59, 11, 13,  3,  8)

    ASMR4(eax, ebx, ecx, edx, ebp, 60, 12, 14,  4,  9)
    ASMR4(ebp, eax, ebx, ecx, edx, 61, 13, 15,  5, 10)
    ASMR4(edx, ebp, eax, ebx, ecx, 62, 14,  0,  6, 11)
    ASMR4(ecx, edx, ebp, eax, ebx, 63, 15,  1,  7, 12)
    ASMR4(ebx, ecx, edx, ebp, eax, 64,  0,  2,  8, 13)
    ASMR4(eax, ebx, ecx, edx, ebp, 65,  1,  3,  9, 14)
    ASMR4(ebp, eax, ebx, ecx, edx, 66,  2,  4, 10, 15)
    ASMR4(edx, ebp, eax, ebx, ecx, 67,  3,  5, 11,  0)
    ASMR4(ecx, edx, ebp, eax, ebx, 68,  4,  6, 12,  1)
    ASMR4(ebx, ecx, edx, ebp, eax, 69,  5,  7, 13,  2)
    ASMR4(eax, ebx, ecx, edx, ebp, 70,  6,  8, 14,  3)
    ASMR4(ebp, eax, ebx, ecx, edx, 71,  7,  9, 15,  4)
    ASMR4(edx, ebp, eax, ebx, ecx, 72,  8, 10,  0,  5)
    ASMR4(ecx, edx, ebp, eax, ebx, 73,  9, 11,  1,  6)
    ASMR4(ebx, ecx, edx, ebp, eax, 74, 10, 12,  2,  7)
    ASMR4(eax, ebx, ecx, edx, ebp, 75, 11, 13,  3,  8)
    ASMR4(ebp, eax, ebx, ecx, edx, 76, 12, 14,  4,  9)
    ASMR4(edx, ebp, eax, ebx, ecx, 77, 13, 15,  5, 10)
    ASMR4(ecx, edx, ebp, eax, ebx, 78, 14,  0,  6, 11)
    ASMR4(ebx, ecx, edx, ebp, eax, 79, 15,  1,  7, 12)


    AS2(    movd  esi, mm1              )   // digest_

    AS2(    add   [esi],      eax       )   // write out
    AS2(    add   [esi +  4], ebx       )
    AS2(    add   [esi +  8], ecx       )
    AS2(    add   [esi + 12], edx       )
    AS2(    add   [esi + 16], ebp       )

    // setup next round
    AS2(    movd  ebp, mm2              )   // times
 
    AS2(    mov   edi, DWORD PTR [esp + 64] )   // data
    
    AS2(    add   edi, 64               )   // next round of data
    AS2(    mov   [esp + 64], edi       )   // restore
    
    AS1(    dec   ebp                   )
    AS2(    movd  mm2, ebp              )
    AS1(    jnz   loopStart             )


    EPILOG()
}


#endif // DO_SHA_ASM

} // namespace
