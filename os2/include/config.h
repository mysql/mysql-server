#include <config-os2.h>
#include <types.h>

#undef  HAVE_POSIX_SIGNALS
#undef  HAVE_BSD_SIGNALS
#define TERMIO_TTY_DRIVER

#define ScreenCols()		80
#define ScreenRows()		25

#define tputs(a,b,c) 		puts(a)
#define kbhit		 	_kbhit
//#define _read_kbd(a, b, c) 	_kbhit()
