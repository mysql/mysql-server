/ rint.s (emx+gcc) -- Copyright (c) 1992-1996 by Eberhard Mattes

#include <emx/asm386.h>

#define FUNC    MATHSUFFIX2(rint)

        .globl  LABEL(FUNC)

        .text

        ALIGN

/ double _rint (double x)

#define cw1       0(%esp)
#define cw2       2(%esp)
/define ret_addr  4(%esp)
#define x         8(%esp)

LABEL(FUNC):
        PROFILE_NOFRAME
        subl    $4, %esp
        fstcww  cw1
        movw    cw1, %ax
        andw    $0xf3ff, %ax            /* round to nearest or even */
        movw    %ax, cw2
        fldcww  cw2
        FLD     x                       /* x */
        frndint
        fldcww  cw1
        addl    $4, %esp
        EPILOGUE(FUNC)
