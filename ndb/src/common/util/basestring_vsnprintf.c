// define on IRIX to get posix complian vsnprintf
#define _XOPEN_SOURCE 500
#include <stdio.h>

int
basestring_vsnprintf(char *str, size_t size, const char *format, va_list ap)
{
  return(vsnprintf(str, size, format, ap));
}
