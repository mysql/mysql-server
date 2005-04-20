/*
   config-readline.h  Maintained by hand. Contains the readline specific
   parts from config.h.in in readline 4.3
*/

#if defined (HAVE_CONFIG_H)
#  include <config.h>
#endif

/*
 Ultrix botches type-ahead when switching from canonical to
   non-canonical mode, at least through version 4.3
*/
#if !defined (HAVE_TERMIOS_H) || !defined (HAVE_TCGETATTR) || defined (ultrix)
#  define TERMIOS_MISSING
#endif

#if defined (STRCOLL_BROKEN)
#  undef HAVE_STRCOLL
#endif

#if defined (__STDC__) && defined (HAVE_STDARG_H)
#  define PREFER_STDARG
#  define USE_VARARGS
#else
#  if defined (HAVE_VARARGS_H)
#    define PREFER_VARARGS
#    define USE_VARARGS
#  endif
#endif
