/*
   Copyright (c) 2000, 2014, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; see the file COPYING. If not, write to the
   Free Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston,
   MA  02110-1301  USA.
*/

/* based on Wei Dai's arc4.cpp from CryptoPP */

#include "runtime.hpp"
#include "arc4.hpp"


#if defined(TAOCRYPT_X86ASM_AVAILABLE) && defined(TAO_ASM)
    #define DO_ARC4_ASM
#endif


namespace TaoCrypt {

void ARC4::SetKey(const byte* key, word32 length)
{
    x_ = 1;
    y_ = 0;

    word32 i;

    for (i = 0; i < STATE_SIZE; i++)
        state_[i] = i;

    word32 keyIndex = 0, stateIndex = 0;

    for (i = 0; i < STATE_SIZE; i++) {
        word32 a = state_[i];
        stateIndex += key[keyIndex] + a;
        stateIndex &= 0xFF;
        state_[i] = state_[stateIndex];
        state_[stateIndex] = a;

        if (++keyIndex >= length)
            keyIndex = 0;
    }
}


// local
namespace {

inline unsigned int MakeByte(word32& x, word32& y, byte* s)
{
    word32 a = s[x];
    y = (y+a) & 0xff;

    word32 b = s[y];
    s[x] = b;
    s[y] = a;
    x = (x+1) & 0xff;

    return s[(a+b) & 0xff];
}

} // namespace



void ARC4::Process(byte* out, const byte* in, word32 length)
{
    if (length == 0) return;

#ifdef DO_ARC4_ASM
    if (isMMX) {
        AsmProcess(out, in, length);
        return;
    } 
#endif

    byte *const s = state_;
    word32 x = x_;
    word32 y = y_;

    if (in == out)
        while (length--)
            *out++ ^= MakeByte(x, y, s);
    else
        while(length--)
            *out++ = *in++ ^ MakeByte(x, y, s);
    x_ = x;
    y_ = y;
}



#ifdef DO_ARC4_ASM

#ifdef _MSC_VER
    __declspec(naked)
#else
    __attribute__ ((noinline))
#endif
void ARC4::AsmProcess(byte* out, const byte* in, word32 length)
{
#ifdef __GNUC__
    #define AS1(x)    #x ";"
    #define AS2(x, y) #x ", " #y ";"

    #define PROLOG()  \
    __asm__ __volatile__ \
    ( \
        ".intel_syntax noprefix;" \
        "push ebx;" \
        "push ebp;" \
        "mov ebp, eax;"
    #define EPILOG()  \
        "pop ebp;" \
        "pop ebx;" \
       	"emms;" \
       	".att_syntax;" \
            : \
            : "c" (this), "D" (out), "S" (in), "a" (length) \
            : "%edx", "memory", "cc" \
    );

#else
    #define AS1(x)    __asm x
    #define AS2(x, y) __asm x, y

    #define PROLOG() \
        AS1(    push  ebp                       )   \
        AS2(    mov   ebp, esp                  )   \
        AS2(    movd  mm3, edi                  )   \
        AS2(    movd  mm4, ebx                  )   \
        AS2(    movd  mm5, esi                  )   \
        AS2(    movd  mm6, ebp                  )   \
        AS2(    mov   edi, DWORD PTR [ebp +  8] )   \
        AS2(    mov   esi, DWORD PTR [ebp + 12] )   \
        AS2(    mov   ebp, DWORD PTR [ebp + 16] )

    #define EPILOG() \
        AS2(    movd  ebp, mm6                  )   \
        AS2(    movd  esi, mm5                  )   \
        AS2(    movd  ebx, mm4                  )   \
        AS2(    movd  edi, mm3                  )   \
        AS2(    mov   esp, ebp                  )   \
        AS1(    pop   ebp                       )   \
        AS1(    emms                            )   \
        AS1(    ret 12                          )
        
#endif

    PROLOG()

    AS2(    sub    esp, 4                   )   // make room 

    AS2(    cmp    ebp, 0                   )
    AS1(    jz     nothing                  )

    AS2(    mov    [esp], ebp               )   // length

    AS2(    movzx  edx, BYTE PTR [ecx + 1]  )   // y
    AS2(    lea    ebp, [ecx + 2]           )   // state_
    AS2(    movzx  ecx, BYTE PTR [ecx]      )   // x

    // setup loop
    // a = s[x];
    AS2(    movzx  eax, BYTE PTR [ebp + ecx]    )


#ifdef _MSC_VER
    AS1( loopStart: )  // loopStart
#else
    AS1( 0: )          // loopStart for some gas (need numeric for jump back 
#endif

    // y = (y+a) & 0xff;
    AS2(    add    edx, eax                     )
    AS2(    and    edx, 255                     )

    // b = s[y];
    AS2(    movzx  ebx, BYTE PTR [ebp + edx]    )

    // s[x] = b;
    AS2(    mov    [ebp + ecx], bl              )

    // s[y] = a;
    AS2(    mov    [ebp + edx], al              )

    // x = (x+1) & 0xff;
    AS1(    inc    ecx                          )
    AS2(    and    ecx, 255                     )

    //return s[(a+b) & 0xff];
    AS2(    add    eax, ebx                     )
    AS2(    and    eax, 255                     )
    
    AS2(    movzx  ebx, BYTE PTR [ebp + eax]    )

    // a = s[x];   for next round
    AS2(    movzx  eax, BYTE PTR [ebp + ecx]    )

    // xOr w/ inByte
    AS2(    xor    bl,  BYTE PTR [esi]          )
    AS1(    inc    esi                          )

    // write to outByte
    AS2(    mov    [edi], bl                    )
    AS1(    inc    edi                          )

    AS1(    dec    DWORD PTR [esp]              )
#ifdef _MSC_VER
    AS1(    jnz   loopStart )  // loopStart
#else
    AS1(    jnz   0b )         // loopStart
#endif


    // write back to x_ and y_
    AS2(    mov    [ebp - 2], cl            )
    AS2(    mov    [ebp - 1], dl            )


AS1( nothing:                           )

    // inline adjust 
    AS2(    add   esp, 4               )   // fix room on stack

    EPILOG()
}

#endif // DO_ARC4_ASM


}  // namespace
