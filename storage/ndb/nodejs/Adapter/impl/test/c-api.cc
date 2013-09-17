/* A simple C API */

#include <string.h>

#include "c-api.h"

int whatnumber(int a, const char *b) { 
  return a + strlen(b);
}

int doubleminus(unsigned int n) {
  return -2 * n;
}

