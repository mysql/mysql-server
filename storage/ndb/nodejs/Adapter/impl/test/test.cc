#include <stdio.h>

#include "c-api.h"
#include "cxx-api.hpp"

main() {
  Point p(3,4);
  Circle c(p, 2);

  printf("whatnumber: %d\n", whatnumber(6,"mercy"));
  printf("quadrant: %d\n", p.quadrant());
  printf("area: %f\n", c.area());
}