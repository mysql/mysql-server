/* A simple C API */

#include <string.h>


int whatnumber(int a, const char *b) { 
  return a + strlen(b);
}
